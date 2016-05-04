#include <arpa/inet.h>
#include <sstream>
#include <stdio.h>
#include <string>
#include <unistd.h>

#include "handle.h"
#include "lang/verify.h"
#include "lock_server_cache.h"
#include "tprintf.h"

static void *
revoker_starter(void* server)
{
  ((lock_server_cache*) server)->revoker();
  return 0;
}

static void *
enforcer_starter(void* server)
{
  ((lock_server_cache*) server)->enforcer();
  return 0;
}

lock_server_cache::lock_server_cache()
{
  pthread_create(&enforcer_t, NULL, &enforcer_starter, this);
  pthread_create(&revoker_t, NULL, &revoker_starter, this);
}

int lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id,
                               int &)
{
  sync_root.lock();

  tprintf("\nGot acquire request for %llu from %s - ", lid, id.c_str());

  // let's see if we have a copy of the lock being requested
  std::map<lock_protocol::lockid_t, cached_lock_info*>::iterator lock_list_it = lock_list.find(lid);

  if (lock_list_it == lock_list.end()) {
    // ok we dont have a copy of this lock, let's make a record of it and grant it
    cached_lock_info* new_lock = new cached_lock_info();

    new_lock->holder = id;

    lock_list[lid] = new_lock;

    tprintf("\nLock %llu is NEW granted to %s - ", lid, id.c_str());

    sync_root.unlock();

    return lock_protocol::OK;
  }

  cached_lock_info* lock = lock_list[lid];

  // the lock is available for grabs
  if (lock->holder == FREE) {
    tprintf("\nLock %llu is FREE granted to %s - ", lid, id.c_str());
    lock->holder = id;

    sync_root.unlock();

    return lock_protocol::OK;
  } else {
    // ok someone has the lock, we have to make them wait
    lock->queue.push_back(id);

       // see if anyone has told the current holder to give the lock back
    if (lock->revoked == false)
    {
       tprintf("\nLock %llu is TAKEN queueing %s - Queue length: %lu -", lid, id.c_str(), lock->queue.size());

      //lock->revoked = true;
      sync_root.unlock();

      //revoke(lid);
    } else {
      // the lock has already been flagged for revoking, we can only wait
      tprintf("\nLock %llu is TAKEN - pending revoke on %s - queueing %s - ", lid, lock->holder.c_str(), id.c_str());

      sync_root.unlock();
    }

    return lock_protocol::RETRY;
  }
}

int
lock_server_cache::release(lock_protocol::lockid_t lid, std::string id,
         int &r)
{
  sync_root.lock();

  cached_lock_info* lock = lock_list[lid];

  lock->revoked = false;

  // check to see if anyone is waiting
if (lock->queue.size() == 0) {
  // nothing to do here folks
  lock->holder = FREE;

  sync_root.unlock();
} else {
  std::string old_holder = lock->holder;
  std::string holder = lock->queue.front();
  lock->queue.pop_front();
  lock->holder = holder;

  sync_root.unlock();

  // call retry here, which in this model is actually granting the lock to the client
  handle h(holder);

  tprintf("\nLock %llu is FREE returned by %s sending RETRY to  %s -  Queue length: %lu - ", lid, old_holder.c_str(), holder.c_str(), lock->queue.size());

  lock_protocol::status ret;

  if (h.safebind())
    ret=h.safebind()->call(rlock_protocol::retry, lid, r);

  if (!h.safebind() || ret != lock_protocol::OK) {
    tprintf("\nSafebind failed!!!!!!!!!!!!!!!!")
  }
}

return lock_protocol::OK;
}

lock_protocol::status
lock_server_cache::stat(lock_protocol::lockid_t lid, int &r)
{
  tprintf("stat request\n");
  r = nacquire;
  return lock_protocol::OK;
}

void lock_server_cache::revoke(lock_protocol::lockid_t lid) {
  int r;
  cached_lock_info* lock = lock_list[lid];

  handle h(lock->holder);

  if (h.safebind())
    h.safebind()->call(rlock_protocol::revoke, lid, r);
}

void
lock_server_cache::enforcer(void) {
  // cleans up anyone who is holding the lock while others are waiting, but
  // hasnt been informed of the revoke
  tprintf("\nReleaser started. ");
  while(true) {
    revoke_lock.lock();
    sync_root.lock();

    typedef std::map<lock_protocol::lockid_t, cached_lock_info*>::iterator it_type;

    for(it_type iterator = lock_list.begin(); iterator != lock_list.end(); iterator++) {
      if (iterator->second->queue.size() !=0 && iterator->second->revoked == false) {
        revoke_request request;
        request.holder = iterator->second->holder;
        request.lid = iterator->first;

        revoke_queue.push_back(request);

        iterator->second->revoked = true;
      }
    }

    sync_root.unlock();
    revoke_lock.unlock();

    // 5 ms
    usleep(5000);
  }
}

void
lock_server_cache::revoker(void) {
  // goes through the list, and actually calls the revoke on the client
  tprintf("\nRevoker started. ");
  int r;

  while (true) {
    revoke_lock.lock();

    while (revoke_queue.size() != 0) {
      revoke_request request = revoke_queue.front();
      revoke_queue.pop_front();

      handle h(request.holder);

      tprintf("\nSending revoke to %s for lock %llu.", request.holder.c_str(), request.lid);

      lock_protocol::status ret;

      if (h.safebind())
        ret = h.safebind()->call(rlock_protocol::revoke, request.lid, r);

      if (!h.safebind() || ret != lock_protocol::OK) {
         tprintf("\nSafebind failed - revoking!!!!!!!!!!!!!!!!");
      }

      tprintf("\nRevoke send sucessfully to %s for lock %llu.", request.holder.c_str(), request.lid);
    }

    revoke_lock.unlock();

    // 5 ms
    usleep(5000);
  }
}
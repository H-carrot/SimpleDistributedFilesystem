// the caching lock server implementation

#include <arpa/inet.h>
#include <sstream>
#include <stdio.h>
#include <string>
#include <unistd.h>

#include "handle.h"
#include "lang/verify.h"
#include "lock_server_cache.h"
#include "tprintf.h"

lock_server_cache::lock_server_cache()
{
}


int lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id,
                               int &)
{
  sync_root.lock();

  // let's see if we have a copy of the lock being requested
  std::map<lock_protocol::lockid_t, cached_lock_info*>::iterator lock_list_it = lock_list.find(lid);

  if (lock_list_it == lock_list.end()) {
    // ok we dont have a copy of this lock, let's make a record of it and grant it
    cached_lock_info* new_lock = new cached_lock_info();

    new_lock->holder = id;

    lock_list[lid] = new cached_lock_info();

    sync_root.unlock();

    return lock_protocol::OK;
  }

  cached_lock_info* lock = lock_list[lid];

  // the lock is available for grabs
  if (lock->holder == FREE) {
    lock->holder = id;

    sync_root.unlock();

    return lock_protocol::OK;
  } else {
    // ok someone has the lock, we have to make them wait
    lock->queue.push_back(id);

    // see if anyone has told the current holder to give the lock back
    if (lock->revoked == false)
    {
      lock->revoked = true;
      sync_root.unlock();

      revoke(lid);
    } else {
      // the lock has already been flagged for revoking, we can only wait
      sync_root.unlock();
    }

    return lock_protocol::RETRY;
  }

  sync_root.unlock();
  return lock_protocol::NOENT;
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
  std::string holder = lock->queue.front();
  lock->queue.pop_front();
  lock->holder = holder;

  sync_root.unlock();

  // call retry here, which in this model is actually granting the lock to the client
  handle h(lock->holder);

  if (h.safebind())
    h.safebind()->call(rlock_protocol::retry, lid, r);
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


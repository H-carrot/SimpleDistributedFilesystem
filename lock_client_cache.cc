// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include "tprintf.h"


lock_client_cache::lock_client_cache(std::string xdst,
				     class lock_release_user *_lu)
  : lock_client(xdst), lu(_lu)
{
  rpcs *rlsrpc = new rpcs(0);
  rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache::revoke_handler);
  rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache::retry_handler);

  const char *hname;
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << rlsrpc->port();
  id = host.str();
}

  // int r;
  // lock_protocol::status ret = cl->call(lock_protocol::stat, cl->id(), lid, r);
  // VERIFY (ret == lock_protocol::OK);
  // return r;

lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid)
{
  int r;
  bool awaiting_lock = true;

  sync_root.lock();

  tprintf("\nAcquiring lock %llu. ", lid);

  // let's see if we have a copy of the lock, and whether we can used the cached version
  std::map<lock_protocol::lockid_t, lock_info*>::iterator lock_list_it = lock_list.find(lid);

  if (lock_list_it == lock_list.end()) {
    // ok we dont have a copy of this lock, let's make a record of it
    lock_info* new_lock = new lock_info();

    new_lock->holder = FREE;
    new_lock->owned = false;
    new_lock->acquiring = false;

    lock_list[lid] = new_lock;

    tprintf("\n%llu is a new lock, creating it. ", lid);
  }

  sync_root.unlock();

  // ok let's try to get the lock
  while (awaiting_lock) {
    sync_root.lock();

    lock_info* lock = lock_list[lid];

    // let people know we are waiting
    lock->awaiting_lock++;

    if (lock->owned == false && lock->acquiring == false) {
      // let's go grab that lock
      sync_root.unlock();

      tprintf("\nAcquiring lock %llu from server. ", lid);
      acquire_lock(lid);
      tprintf("\nAcquiring lock %llu from server submitted. ", lid);

      sync_root.lock();
    }

    // wait until our client has the lock
    if (lock->owned == false) {
      while (lock->owned == false)
        lock->flag.wait(&sync_root);
    }

    tprintf("\nLock %llu is owned by this client. ", lid);

    if (lock->holder == FREE) {
      // that was easy, we got the lock
      lock->holder = pthread_self();
      lock->awaiting_lock--;

       tprintf("\nGot lock %llu, was free. ", lid);

      break;
    } else {
      // someone on our client has the lock
      lock->awaiting_lock++;

       tprintf("\nWaiting for thread to release lock %llu. ", lid);

      while (lock->holder != FREE)
        lock->flag.wait(&sync_root);

      // ok now we have the lock
      lock->awaiting_lock--;
      lock->holder = pthread_self();

      tprintf("\nLock %llu acquired sucessfully. ", lid);

      break;
    }
  }

  sync_root.unlock();

  return lock_protocol::OK;
}

lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid)
{
  int r;

  sync_root.lock();

  tprintf("\nReleasing lock %llu. ", lid);

  lock_info* lock = lock_list[lid];

  lock->holder = FREE;

  if (lock->awaiting_lock == 0 && lock->revoked == true) {
    // ok so no one on our client wants the lock and the server wants it back, so let's return it
    lock->owned = false;
    lock->revoked = false;
    lock->holder = FREE;

    sync_root.unlock();

    tprintf("\nLock %llu revoked by server, no one waiting, returning. ", lid);

    cl->call(lock_protocol::release, lid, id, r);

    tprintf("\nLock %llu returned to server. ", lid);

    return lock_protocol::OK;
  } else {
    // either the lock wasnt revoked, or someone on the client still wants it

    lock->holder = FREE;

    tprintf("\nLock %llu released. Awaiting lock: %d. ", lid, lock->awaiting_lock);

    if (lock->awaiting_lock != 0) {
      // wake up any waiting clients
      lock->flag.signalAll();
    }

    sync_root.unlock();

    return lock_protocol::OK;
  }
}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid,
                                  int &)
{
  int r;
  sync_root.lock();

  tprintf("\nServer is signaling revoke of lock %llu.", lid);

  // let's see if we have a copy of the lock, and whether we can used the cached version
  std::map<lock_protocol::lockid_t, lock_info*>::iterator lock_list_it = lock_list.find(lid);

  if (lock_list_it == lock_list.end()) {
    // ok we dont have a copy of this lock, let's make a record of it
    lock_info* new_lock = new lock_info();

    new_lock->holder = FREE;

    lock_list[lid] = new_lock;
  }

  lock_info* lock = lock_list[lid];

  if (lock->holder == FREE && lock->awaiting_lock == 0) {
    lock->owned = false;

    tprintf("\nLock %llu revoked immediatly, no holders..", lid);

    sync_root.unlock();

    // we need to do a release here
    cl->call(lock_protocol::release, lid, id, r);

    return lock_protocol::OK;
  } else {
    // ok either someone still has the lock or still wanted it on this client, so we will wait
    // to actually return the lock to the server
    lock->revoked = true;

    tprintf("\nLock %llu revoke pending.", lid);

    sync_root.unlock();

    return lock_protocol::OK;
  }
}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid,
                                 int &)
{
  // if this is called, it means the sever is signaling that we have the lock
  sync_root.lock();

  tprintf("\nLock %llu has been granted to client via RETRY.", lid);

  lock_info* lock = lock_list[lid];

  lock->acquiring = false;
  lock->owned = true;

  lock->flag.signalAll();

  sync_root.unlock();

  return rlock_protocol::OK;
}

// just grabs the lock and updates the status
void lock_client_cache::acquire_lock(lock_protocol::lockid_t lid) {
  int r;
  lock_protocol::status ret;

  sync_root.lock();

  lock_info* lock = lock_list[lid];

  // make sure somone didnt already try grabbing the lock
  if (lock->acquiring == false && lock->owned == false){
    lock->acquiring = true;
  } else {
    sync_root.unlock();
    return;
  }

  sync_root.unlock();

  ret = cl->call(lock_protocol::acquire, lid, id, r);

  sync_root.lock();

  // if we get an ok, we have the lock, if we dont
  // it means that we have to wait
  if (ret == lock_protocol::OK) {
    lock->acquiring = false;
    lock->owned = true;
  } else {
    lock->acquiring = true;
    lock->owned = false;
  }

  sync_root.unlock();
}

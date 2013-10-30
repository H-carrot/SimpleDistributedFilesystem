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
    new_lock->awaiting_lock = 0;

    lock_list[lid] = new_lock;

    tprintf("\n%llu is a new lock, creating it. ", lid);
  }

  sync_root.unlock();

  // ok let's try to get the lock
  sync_root.lock();

  lock_info* lock = lock_list[lid];

  // let people know we are waiting
  lock->awaiting_lock++;

  if (lock->owned == false && lock->acquiring == false) {
    // let's go grab that lock
    sync_root.unlock();

    tprintf("\nAcquiring lock %llu from server. ", lid);
    acquire_lock(lid);
    tprintf("\nAcquiring lock %llu from server submitted by %s ", lid, id.c_str());

    sync_root.lock();
  }

  // wait until our client has the lock
  if (lock->owned == false) {
    while (lock->owned == false)
      lock->flag.wait(&sync_root);
  }

  tprintf("\nLock %llu is owned by this client %s - ", lid, id.c_str());

  lock = lock_list[lid];

  if (lock->holder == FREE) {
    // that was easy, we got the lock
    lock->holder = pthread_self();
    lock->awaiting_lock--;

    tprintf("\nGot lock %llu, was free. ", lid);

  } else {
    while (lock->holder != FREE) {
      tprintf("\nWaiting for thread to release lock %llu on %s. ", lid, id.c_str());
      lock->flag.wait(&sync_root);
    }


    // ok now we have the lock
    lock->awaiting_lock--;
    lock->holder = pthread_self();

    tprintf("\nLock %llu acquired by thread sucessfully on %s, awaiting_lock: %d . ", lid, id.c_str(), lock->awaiting_lock);
  }

  sync_root.unlock();

  return lock_protocol::OK;
}

lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid)
{
  int r;

  sync_root.lock();

  lock_info* lock = lock_list[lid];

  tprintf("\nReleasing lock %llu on %s. Awaiting lock: %d - ", lid, id.c_str(), lock->awaiting_lock);

  lock->holder = FREE;

  if (lock->awaiting_lock == 0 && lock->revoked == true) {
    // ok so no one on our client wants the lock and the server wants it back, so let's return it
    lock->owned = false;
    lock->revoked = false;
    lock->acquiring = false;
    lock->holder = FREE;

    sync_root.unlock();

    tprintf("\nLock %llu revoked by server, no one waiting, returning. ", lid);

    cl->call(lock_protocol::release, lid, id, r);

    tprintf("\nLock %llu returned to server. ", lid);

    return lock_protocol::OK;
  } else {
    // either the lock wasnt revoked, or someone on the client still wants it
    tprintf("\nLock %llu released on %s. Awaiting lock: %d. ", lid, id.c_str(), lock->awaiting_lock);

    if (lock->awaiting_lock != 0) {
      tprintf("\nLock %llu released. Signaling clients: %d - ", lid, lock->awaiting_lock);
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

  tprintf("\nServer is signaling revoke of lock %llu to this client %s. ", lid, id.c_str());

  // let's see if we have a copy of the lock, and whether we can used the cached version
  std::map<lock_protocol::lockid_t, lock_info*>::iterator lock_list_it = lock_list.find(lid);

  if (lock_list_it == lock_list.end()) {
    tprintf("\nLock %llu doesnt exist on this client (%s) yet. Marking it as revoked in future.", lid, id.c_str());
    // ok we dont have a copy of this lock, let's make a record of it
    lock_info* new_lock = new lock_info();

    // because of our protocol, we need to essentially create a new record and give the lock to
    // the server until it is done with it
    new_lock->holder = FREE;
    new_lock->revoked = true;
    new_lock->owned = true;
    new_lock->awaiting_lock=0;

    lock_list[lid] = new_lock;

    sync_root.unlock();
    return lock_protocol::OK;
  }

  lock_info* lock = lock_list[lid];

  if (lock->holder == FREE && lock->awaiting_lock == 0 && lock->acquiring == false) {
    lock->owned = false;
    lock->revoked = false;

    if (lock->acquiring) {
      tprintf("\n!!!!!!!!!!!!!!!-------!!!!!!!!!!!!!!!!!!!!!----!!!!!");
      tprintf("\nLock acquiring but awaiting_lock set to 0");
      tprintf("\n!!!!!!!!!!!!!!!-------!!!!!!!!!!!!!!!!!!!!!----!!!!!");
    }

    tprintf("\nLock %llu revoked immediatly from %s, no holders..", lid, id.c_str());

    sync_root.unlock();

    // we need to do a release here
    cl->call(lock_protocol::release, lid, id, r);

    return lock_protocol::OK;
  } else {
    // ok either someone still has the lock or still wanted it on this client, so we will wait
    // to actually return the lock to the server
    lock->revoked = true;

    tprintf("\nLock %llu revoke pending. %s and %s and %d waiting.",
      lid,
      lock->holder==FREE ? "FREE" :  "NOT FREE",
      lock->owned ==true ? "OWNED" : "NOT OWNED",
      lock->awaiting_lock);

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

  lock_info* lock = lock_list[lid];

  tprintf("\nLock %llu has been granted to client %s via RETRY and status %s and %s. Awaiting lock: %d - ", lid,
    id.c_str(),
    lock->holder==FREE ? "FREE" :  "NOT FREE",
    lock->owned ==true ? "OWNED" : "NOT OWNED",
    lock->awaiting_lock);

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

  if (lock->owned == true) {
    // ok it looks like a retry came in after us, we dont need to do anything
    lock->flag.signalAll();
    tprintf("\nLock %llu has been granted to %s client. RETRY beat ACQUIRE.", lid, id.c_str());
    sync_root.unlock();
    return;
  }
  // if we get an ok, we have the lock, if we dont
  // it means that we have to wait
  if (ret == lock_protocol::OK) {
    tprintf("\nLock %llu has been granted to client %s via ACQUIRE.", lid, id.c_str());
    lock->acquiring = false;
    lock->owned = true;
    lock->flag.signalAll();
  } else {
    lock->acquiring = true;
    lock->owned = false;
  }

  sync_root.unlock();
}

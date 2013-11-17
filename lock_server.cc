// the lock server implementation
#include <arpa/inet.h>
#include <sstream>
#include <stdio.h>
#include <unistd.h>

#include "lock_server.h"

lock_server::lock_server():
  nacquire (0)
{
  printf("Lock server starting...\n\n");
}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("stat request from clt %d\n", clt);
  r = nacquire;
  return ret;
}

lock_protocol::status lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r) {

  // first grab the global lock, as we might have a new lock request non seen
  // if that is the case, add it, if
  sync_root.lock();

  printf("\nAcquiring lock: %d", lid);

  std::map<lock_protocol::lockid_t, lock_info*>::iterator it = lock_list.find(lid);

  // a new lock, let's add it
  if (it == lock_list.end()) {
    lock_info* newLock = new lock_info();
    newLock->acquire_count = 1;
    newLock->clt = clt;

    lock_list.insert(std::make_pair(lid, newLock));

  } else {
     // the lock already exists, let's see if we can grab it

    while (it->second->clt != UNLOCKED)
      flag.wait(&sync_root);

    it->second->acquire_count++;
    it->second->clt = clt;
  }

  flag.signalAll();
  sync_root.unlock();

  // if we didnt get the lock we have to try again, ideally this would be a condition variable
  // need to implement that when I have some time.

  return lock_protocol::OK;
}

lock_protocol::status lock_server::release(int clt, lock_protocol::lockid_t lid, int &r) {
  sync_root.lock();

  printf("\nReleasing lock: %d...", lid);

  std::map<lock_protocol::lockid_t, lock_info*>::iterator it = lock_list.find(lid);

  // make sure that it is locked and by the current client
  if (it->second->clt == clt) {
    printf("released.");
    it->second->clt = UNLOCKED;
  } else {
    printf("never owned.");
    sync_root.unlock();
    return lock_protocol::IOERR;
  }

  flag.signalAll();
  sync_root.unlock();

  return lock_protocol::OK;
}
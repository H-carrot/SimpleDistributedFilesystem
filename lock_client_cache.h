// lock client interface.

#ifndef lock_client_cache_h

#define lock_client_cache_h

#include <string>

#include "lang/verify.h"
#include "lock.hpp"
#include "lock_client.h"
#include "lock_protocol.h"
#include "rpc.h"

#define FREE 99999999

struct lock_info {
  base::ConditionVar flag;      // condition variable for threads waiting for the lock
  lock_protocol::lockid_t lid;  // the id of the lock
  volatile bool acquiring;      // if the lock is currently being acquired by another thread
  volatile bool owned;          // if the lock is currently owned by this client
  volatile bool revoked;        // if the lock is pending revoke by the lock server
  volatile int awaiting_lock;   // number of threads waiting for the lock
  volatile pthread_t holder;    // the id of the pthread currently holding the lock
};

// Classes that inherit lock_release_user can override dorelease so that 
// that they will be called when lock_client releases a lock.
// You will not need to do anything with this class until Lab 5.
class lock_release_user {
 public:
  virtual void dorelease(lock_protocol::lockid_t) = 0;
  virtual ~lock_release_user() {};
};

class lock_client_cache : public lock_client {
 private:
  class lock_release_user *lu;
  int rlock_port;
  std::string hostname;
  std::string id;
  std::map<lock_protocol::lockid_t, lock_info*> lock_list;
  base::Mutex sync_root;

  void acquire_lock(lock_protocol::lockid_t);
 public:
  lock_client_cache(std::string xdst, class lock_release_user *l = 0);
  virtual ~lock_client_cache() {};
  lock_protocol::status acquire(lock_protocol::lockid_t);
  lock_protocol::status release(lock_protocol::lockid_t);
  rlock_protocol::status revoke_handler(lock_protocol::lockid_t, 
                                        int &);
  rlock_protocol::status retry_handler(lock_protocol::lockid_t, 
                                       int &);
};

#endif

#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <list>
#include <map>
#include <string>
#include <unistd.h>

#include "lock.hpp"
#include "lock_protocol.h"
#include "lock_server.h"
#include "rpc.h"

#define FREE "MMMMM"

struct cached_lock_info {
  std::string holder;           // the client holding the lock
  volatile bool revoked;        // if the client has been sent a revoke message asking for the lock back
  std::list<std::string> queue; // list of clients waiting for the lock
  base::ConditionVar flag;      // condition variable for threads waiting for the lock
};

struct revoke_request {
  std::string holder;
  lock_protocol::lockid_t lid;
};

class lock_server_cache {
 private:
  int nacquire;
  base::Mutex sync_root;
  base::Mutex revoke_lock;
  std::map<lock_protocol::lockid_t, cached_lock_info*> lock_list;
  std::list<revoke_request> revoke_queue;
  pthread_t enforcer_t;
  pthread_t revoker_t;

 public:
  lock_server_cache();
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  int acquire(lock_protocol::lockid_t, std::string id, int &);
  int release(lock_protocol::lockid_t, std::string id, int &);
  void revoke(lock_protocol::lockid_t);
  void enforcer();
  void revoker();
};

#endif

#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <list>
#include <map>
#include <string>

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

class lock_server_cache {
 private:
  int nacquire;
  base::Mutex sync_root;
  std::map<lock_protocol::lockid_t, cached_lock_info*> lock_list;

 public:
  lock_server_cache();
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  int acquire(lock_protocol::lockid_t, std::string id, int &);
  int release(lock_protocol::lockid_t, std::string id, int &);
  void revoke(lock_protocol::lockid_t);
};

#endif

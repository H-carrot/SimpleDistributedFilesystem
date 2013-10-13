// this is the lock server
// the lock client has a similar interface

#ifndef lock_server_h
#define lock_server_h

#include <map>
#include <string>

#include "lock.hpp"
#include "lock_client.h"
#include "lock_protocol.h"
#include "rpc.h"

#define UNLOCKED 999999999

struct lock_info {
  int acquire_count; // the number of times the lock as been acquired
  volatile int clt;  // the client holding the lock
};


class lock_server {

  protected:
    int nacquire;

  public:
    lock_server();
    ~lock_server() {};
    lock_protocol::status stat(int clt, lock_protocol::lockid_t lid, int &);
    lock_protocol::status acquire(int clt, lock_protocol::lockid_t lid, int &);
    lock_protocol::status release(int clt, lock_protocol::lockid_t lid, int &);

  private:
    base::Mutex sync_root;
    base::ConditionVar flag;
    std::map<lock_protocol::lockid_t, lock_info*> lock_list;
};

#endif

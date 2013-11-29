// extent client interface.

#ifndef extent_client_h
#define extent_client_h

#include <map>
#include <string>
#include "extent_protocol.h"
#include "rpc.h"
#include "lock.hpp"

struct cached_extent_info {
  bool is_dirty;
  extent_protocol::attr attr;
  extent_protocol::extentid_t eid;
  std::string buf;
};

class extent_client {
 private:
  rpcc *cl;
  base::Mutex sync_root;
  std::map<extent_protocol::extentid_t, cached_extent_info*> extent_list;

 public:
  extent_client(std::string dst);

  extent_protocol::status get(extent_protocol::extentid_t eid,
			      std::string &buf);
  extent_protocol::status getattr(extent_protocol::extentid_t eid,
				  extent_protocol::attr &a);
  extent_protocol::status put(extent_protocol::extentid_t eid, std::string buf);
  extent_protocol::status remove(extent_protocol::extentid_t eid);
  extent_protocol::status flush(extent_protocol::extentid_t eid);
};

#endif


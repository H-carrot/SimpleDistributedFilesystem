// this is the extent server

#ifndef extent_server_h
#define extent_server_h

#include <map>
#include <string>

#include "extent_protocol.h"
#include "lock.hpp"

struct file_inode {
  extent_protocol::attr       attributes;
  extent_protocol::extentid_t id;
  std::string                 file_name;
};

class extent_server {

 public:
  extent_server();

  int get(extent_protocol::extentid_t id, std::string &);
  int getattr(extent_protocol::extentid_t id, extent_protocol::attr &);
  int put(extent_protocol::extentid_t id, std::string, int &);
  int remove(extent_protocol::extentid_t id, int &);

private:
  base::Mutex _sync_root;
  std::map<extent_protocol::extentid_t, file_inode*> _extent_map;
};

#endif








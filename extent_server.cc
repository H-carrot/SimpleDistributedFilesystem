// the extent server implementation

#include <fcntl.h>
#include <sstream>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "extent_server.h"

extent_server::extent_server() {}


int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &)
{
  _sync_root.lock();

 std::map<extent_protocol::extentid_t, file_inode*>::iterator it = _extent_map.find(id);

  // a new id, let's add it
  if (it == _extent_map.end()) {
    file_inode* new_file = new file_inode();

    new_file->id = id;
    new_file->file_buf = buf;
    new_file->attributes.size = buf.length();

    // god forbid we use time_t
    new_file->attributes.atime = time(NULL);
    new_file->attributes.ctime = time(NULL);
    new_file->attributes.mtime = time(NULL);

    _extent_map.insert(std::make_pair(id, new_file));
  } else {
    // the id already exists, let's update it's data
    it->second->file_buf = buf;
    it->second->attributes.size = buf.length();

    it->second->attributes.atime = time(NULL);
    it->second->attributes.mtime = time(NULL);
  }

  _sync_root.unlock();
  return extent_protocol::OK;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf)
{
  _sync_root.lock();

  std::map<extent_protocol::extentid_t, file_inode*>::iterator it = _extent_map.find(id);

  if (it == _extent_map.end()) {
    // the id doesnt exist
    _sync_root.unlock();
    return extent_protocol::IOERR;
  }

  // return the buffer value
  buf = it->second->file_buf;

  // update our access timestamp
  it->second->attributes.atime = time(NULL);

  _sync_root.unlock();

  return extent_protocol::OK;
}

int extent_server::getattr(extent_protocol::extentid_t id, extent_protocol::attr &a)
{
  _sync_root.lock();

  std::map<extent_protocol::extentid_t, file_inode*>::iterator it = _extent_map.find(id);

  if (it == _extent_map.end()) {
    // the id doesnt exist
    _sync_root.unlock();

    // initialize the variables to something just incase
    a.atime = 0;
    a.ctime = 0;
    a.mtime = 0;
    a.size  = 0;

    return extent_protocol::IOERR;
  }

  a.atime = it->second->attributes.atime;
  a.ctime = it->second->attributes.ctime;
  a.mtime = it->second->attributes.mtime;
  a.size  = it->second->attributes.size;

  _sync_root.unlock();

  return extent_protocol::OK;
}

int extent_server::remove(extent_protocol::extentid_t id, int &)
{
   _sync_root.lock();

  std::map<extent_protocol::extentid_t, file_inode*>::iterator it = _extent_map.find(id);

  if (it == _extent_map.end()) {
    // the id doesnt exist
    _sync_root.unlock();

    return extent_protocol::IOERR;
  }

  // cleanup the file node
  delete it->second;
  _extent_map.erase(it);

  _sync_root.unlock();

  return extent_protocol::OK;
}
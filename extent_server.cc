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

  file_inode* new_file = (file_inode*)malloc(sizeof(file_inode));

  new_file->id = id;
  new_file->file_name = buf;
  new_file->attributes.size = buf.length();

  // god forbid we use good code like time_t
  new_file->attributes.atime = time(NULL);
  new_file->attributes.ctime = time(NULL);
  new_file->attributes.mtime = time(NULL);

  _sync_root.unlock();
  return extent_protocol::IOERR;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf)
{
  // You fill this in for Lab 2.
  return extent_protocol::IOERR;
}

int extent_server::getattr(extent_protocol::extentid_t id, extent_protocol::attr &a)
{
  // You fill this in for Lab 2.
  // You replace this with a real implementation. We send a phony response
  // for now because it's difficult to get FUSE to do anything (including
  // unmount) if getattr fails.
  a.size = 0;
  a.atime = 0;
  a.mtime = 0;
  a.ctime = 0;
  return extent_protocol::OK;
}

int extent_server::remove(extent_protocol::extentid_t id, int &)
{
  // You fill this in for Lab 2.
  return extent_protocol::IOERR;
}

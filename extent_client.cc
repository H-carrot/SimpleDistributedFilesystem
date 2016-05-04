// RPC stubs for clients to talk to extent_server

#include "extent_client.h"
#include "tprintf.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

// The calls assume that the caller holds a lock on the extent

extent_client::extent_client(std::string dst)
{
  sockaddr_in dstsock;
  make_sockaddr(dst.c_str(), &dstsock);
  cl = new rpcc(dstsock);
  if (cl->bind() != 0) {
    printf("extent_client: bind failed\n");
  }
}

extent_protocol::status
extent_client::get(extent_protocol::extentid_t eid, std::string &buf)
{
  extent_protocol::status ret = extent_protocol::OK;

  tprintf("Getting extent: %llu\n", eid);

  sync_root.lock();

  // let's see if we have a copy of the lock being requested
  std::map<extent_protocol::extentid_t, cached_extent_info*>::iterator extent_list_it = extent_list.find(eid);

  if (extent_list_it == extent_list.end()) {
    tprintf("Extent %llu doesnt exist locally, getting it...\n", eid);

    cached_extent_info* new_extent = new cached_extent_info();

    // ok we dont have a copy, go request it from the server
    ret = cl->call(extent_protocol::get, eid, buf);

    new_extent->eid = eid;
    cl->call(extent_protocol::getattr, eid, new_extent->attr);
    new_extent->is_dirty = false;
    new_extent->attr.atime = time(NULL);

    new_extent->buf=buf;

    tprintf("Got extent %llu, value: %s\n", eid, new_extent->buf.c_str());

    extent_list[eid] = new_extent;

  } else {
    // ok we have a copy locally
    cached_extent_info* extent = extent_list[eid];
    
    buf = extent->buf;

    tprintf("Extent %llu exists locally, value: %s\n", eid, buf.c_str());

    // update the access time
    extent->attr.atime = time(NULL);
  }

  sync_root.unlock();

  return ret;
}

extent_protocol::status
extent_client::getattr(extent_protocol::extentid_t eid,
		       extent_protocol::attr &attr)
{
  extent_protocol::status ret = extent_protocol::OK;

  sync_root.lock();

  // let's see if we have a copy of the lock being requested
  std::map<extent_protocol::extentid_t, cached_extent_info*>::iterator extent_list_it = extent_list.find(eid);

  if (extent_list_it == extent_list.end()) {
    cached_extent_info* new_extent = new cached_extent_info();

    // ok we dont have a copy, go request it from the server
    ret = cl->call(extent_protocol::get, eid, new_extent->buf);

    new_extent->eid = eid;
    ret = cl->call(extent_protocol::getattr, eid, new_extent->attr);
    new_extent->is_dirty = false;

    extent_list[eid] = new_extent;
  }

  // ok we know we have the extent at this point, so return the attributes
  cached_extent_info* extent = extent_list[eid];

  attr = extent->attr;

  sync_root.unlock();

  return ret;
}

extent_protocol::status
extent_client::put(extent_protocol::extentid_t eid, std::string buf)
{
  extent_protocol::status ret = extent_protocol::OK;

  sync_root.lock();

  // make sure we have the extent first, not that we should be doing a blind write
  std::map<extent_protocol::extentid_t, cached_extent_info*>::iterator extent_list_it = extent_list.find(eid);

  if (extent_list_it == extent_list.end()) {
    cached_extent_info* new_extent = new cached_extent_info();

    // ok we dont have a copy, go request it from the server
    cl->call(extent_protocol::get, eid, new_extent->buf);

    new_extent->eid = eid;
    ret = cl->call(extent_protocol::getattr, eid, new_extent->attr);

    extent_list[eid] = new_extent;
  }

  // ok we know we have the extent at this point, so return the attributes
  cached_extent_info* extent = extent_list[eid];

  extent->is_dirty = true;

  tprintf("Updating extent %llu old value: %s, new value: %s\n", eid, extent->buf.c_str(), buf.c_str());

  // now let's update it's attributes
  extent->buf = buf;
  extent->attr.size = buf.length();

  extent->attr.atime = time(NULL);
  extent->attr.ctime = time(NULL);
  extent->attr.mtime = time(NULL);

  sync_root.unlock();

  return ret;
}

extent_protocol::status
extent_client::remove(extent_protocol::extentid_t eid)
{
  extent_protocol::status ret = extent_protocol::OK;
  int r;

  sync_root.lock();

  tprintf("Removing extent %llu\n", eid);

  std::map<extent_protocol::extentid_t, cached_extent_info*>::iterator extent_list_it = extent_list.find(eid);

  if (extent_list_it == extent_list.end()) { 
    // the extent doesnt exist
    sync_root.unlock();
    return ret;
  }

  extent_list.erase(eid);

  ret = cl->call(extent_protocol::remove, eid, r);

  sync_root.unlock();
  return ret;
}

extent_protocol::status
extent_client::flush(extent_protocol::extentid_t eid) {
  extent_protocol::status ret = extent_protocol::OK;
  int r;

  tprintf("Flushing extent id: %llu\n", eid);

  sync_root.lock();

  // make sure we have the extent first, not that we should be doing a blind write
  std::map<extent_protocol::extentid_t, cached_extent_info*>::iterator extent_list_it = extent_list.find(eid);

  if (extent_list_it == extent_list.end()) { 
    // the extent doesnt exist
    sync_root.unlock();
    return ret;
  }

  // load the extent
  cached_extent_info* extent = extent_list[eid];

  if (extent->is_dirty) {
    // ok we need to flush this bad boy back
    tprintf("Flushing %llu... new value: %s\n", eid, extent->buf.c_str());
    cl->call(extent_protocol::put, eid, extent->buf, r);
    tprintf("Flushed %llu.", eid);
  } else {
    tprintf("Flushed - not dirty.\n");
  }

  // at this point we have either flushed it or dont need to because it wasnt dirty
  extent_list.erase(eid);

  sync_root.unlock();

  return ret;
}



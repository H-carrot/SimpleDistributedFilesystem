// yfs client.  implements FS operations using extent and lock server
#include "extent_client.h"
#include "lock_client_cache.h"
#include "StringTokenizer.h"
#include "tprintf.h"
#include "yfs_client.h"

#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
  ec = new extent_client(extent_dst);
  lu = new lock_releaser(ec);
  lc = new lock_client_cache(lock_dst, lu);
  srandom(getpid());
}

yfs_client::inum
yfs_client::n2i(std::string n)
{
  std::istringstream ist(n);
  unsigned long long finum;
  ist >> finum;
  return finum;
}

std::string
yfs_client::filename(inum inum)
{
  std::ostringstream ost;
  ost << inum;
  return ost.str();
}

bool
yfs_client::isfile(inum inum)
{
  if(inum & 0x80000000)
    return true;
  return false;
}

bool
yfs_client::isdir(inum inum)
{
  return ! isfile(inum);
}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
  int r = OK;
  // You modify this function for Lab 3
  // - hold and release the file lock

  lc->acquire(inum);

  printf("getfile %016llx\n", inum);
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    r = IOERR;
    goto release;
  }

  fin.atime = a.atime;
  fin.mtime = a.mtime;
  fin.ctime = a.ctime;
  fin.size = a.size;
  printf("getfile %016llx -> sz %llu\n", inum, fin.size);

 release:
  lc->release(inum);

  return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
  int r = OK;
  // You modify this function for Lab 3
  // - hold and release the directory lock

  lc->acquire(inum);

  printf("getdir %016llx\n", inum);
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    r = IOERR;
    goto release;
  }
  din.atime = a.atime;
  din.mtime = a.mtime;
  din.ctime = a.ctime;

 release:
  lc->release(inum);

  return r;
}

int yfs_client::createFile(yfs_client::inum &inum, yfs_client::inum parent, const char* buf) {
  std::string parent_buf;

  lc->acquire(parent);

  setbuf(stdout, NULL);
  // verify that the parent directory actually exists
  if (ec->get(parent, parent_buf) != extent_protocol::OK) {
      return NOENT;
  }

  // ok the parent exists
  std::list<yfs_client::dirent*>* contents = parsebuf(parent_buf);

  // check for duplicate names
  for (std::list<yfs_client::dirent*>::iterator it = contents->begin();
       it != contents->end();
       it++) {
    if ((*it)->name.compare(buf) == 0 && isfile((*it)->inum)) {
      lc->release(parent);
      return EEXIST;
    }
  }

  // ok there are no files with the same name in this directory
  // we can go ahead and append our new file now to the parent
  // add the seperator if there are multiple items in here
  if (contents->size() !=0 )
    parent_buf += ELEMENTSEPERATOR;

  // generate an inum for our new file
  inum = random();
  inum = inum | 0x80000000; // set the 31 bit correctly

  parent_buf.append(createBuffElement(inum, buf));

  ec->put(inum, "");
  ec->put(parent, parent_buf);

  lc->release(parent);

  return OK;
}

int yfs_client::createDirectory(yfs_client::inum &inum, yfs_client::inum parent, const char* buf) {
  std::string parent_buf;

  lc->acquire(parent);

  setbuf(stdout, NULL);
  // verify that the parent directory actually exists
  if (ec->get(parent, parent_buf) != extent_protocol::OK) {
      lc->release(parent);
      return NOENT;
  }

  // ok the parent exists
  std::list<yfs_client::dirent*>* contents = parsebuf(parent_buf);

  // check for duplicate names
  for (std::list<yfs_client::dirent*>::iterator it = contents->begin();
       it != contents->end();
       it++) {
    if ((*it)->name.compare(buf) == 0 && !isfile((*it)->inum)) {
      lc->release(parent);
      return EEXIST;
    }
  }

  // ok there are no directories with the same name in this directory
  // we can go ahead and append our new directory now to the parent
  // add the seperator if there are multiple items in here
  if (contents->size() !=0 )
    parent_buf += ELEMENTSEPERATOR;

  // generate an inum for our new file
  inum = random();
  inum = inum & 0x7FFFFFFF; // set the 31 bit correctly

  parent_buf.append(createBuffElement(inum, buf));

  ec->put(inum, "");
  ec->put(parent, parent_buf);

  lc->release(parent);

  return OK;
}

int yfs_client::unlinkFile(yfs_client::inum parent, const char* buf) {
  bool foundFile = false;
  std::string parent_buf;
  yfs_client::inum fileInum;

  lc->acquire(parent);

  setbuf(stdout, NULL);
  // verify that the parent directory actually exists
  if (ec->get(parent, parent_buf) != extent_protocol::OK) {
      lc->release(parent);
      return NOENT;
  }

  // ok the parent exists
  std::list<yfs_client::dirent*>* contents = parsebuf(parent_buf);

  // check for duplicate names
  for (std::list<yfs_client::dirent*>::iterator it = contents->begin();
       it != contents->end();
       it++) {
    if ((*it)->name.compare(buf) == 0 && isfile((*it)->inum)) {
      // ok the file exists
      fileInum = (*it)->inum;

      // get the lock on the file so no one can modify it at this point
      lc->acquire(fileInum);

      // remove the element
      contents->erase(it);

      foundFile = true;
      break;
    }
  }

  if (!foundFile) {
    lc->release(parent);
    return NOENT;
  } else {
    printf("\n\nDone searching, file exists...\n\n");
  }

  ec->put(parent, writeDirent(contents));
  ec->remove(fileInum);

  lc->release(fileInum);
  lc->release(parent);

  return OK;
}

int yfs_client::lookupResource(yfs_client::inum &inum, yfs_client::inum parent, const char* buf) {
  std::string parent_buf;
  inum = 0;

  lc->acquire(parent);

  // verify that the parent directory actually exists
  if (ec->get(parent, parent_buf) != extent_protocol::OK) {
      lc->release(parent);
      return IOERR;
  }

  // ok the parent exists
  std::list<yfs_client::dirent*>* contents = parsebuf(parent_buf);

  // scan for the file, and grab the inum if we have it
  for (std::list<yfs_client::dirent*>::iterator it = contents->begin();
       it != contents->end();
       it++) {
    if ((*it)->name.compare(buf) == 0) {
      inum = (*it)->inum;
      break;
    }
  }

  lc->release(parent);
  return OK;
}

int yfs_client::getDirContents(yfs_client::inum inum, std::string &buf) {
  lc->acquire(inum);
  if ((ec->get(inum, buf) == extent_protocol::OK)) {
    lc->release(inum);
    return OK;
  } else {
    lc->release(inum);
    return IOERR;
  }
}

int yfs_client::setAttr(yfs_client::inum inum, yfs_client::fileinfo file_info) {
  extent_protocol::attr attr;
  std::string buf;

  lc->acquire(inum);

  if (ec->getattr(inum, attr) == extent_protocol::OK) {
    if (attr.size > file_info.size) {
      // make the buffer larger
      ec->get(inum, buf);

      buf.resize(file_info.size, '\0');
    } else if ( attr.size < file_info.size) {
      // make it smaller
      ec->get(inum, buf);

      buf.resize(file_info.size);
    } else {
      // same size, no change
      lc->release(inum);
      return OK;
    }

    // update the value
    ec->put(inum, buf);

    lc->release(inum);

    return OK;
  }

  lc->release(inum);


  // couldnt get the attribute
  return IOERR;
}

int yfs_client::writeFile(yfs_client::inum inum, const char* buf, size_t size , off_t off) {
  std::string current_val;

  lc->acquire(inum);

  if (ec->get(inum, current_val) == extent_protocol::OK) {
    // check to see if we need to make the file bigger
    if (off + size > current_val.length()) {
      //yup, lets make this bigger
      current_val.resize(off + size , '\0');
    }

    current_val.replace(off, size, buf, size);

    ec->put(inum, current_val);

    lc->release(inum);

    return OK;
  }

  lc->release(inum);

  return IOERR;
}

int yfs_client::readFile(yfs_client::inum inum, std::string& buf, size_t size , off_t off) {
  std::string current_val;

  lc->acquire(inum);

  if (ec->get(inum, current_val) == extent_protocol::OK) {
    if (off > current_val.length()) {
      // there is nothing we can do here
      lc->release(inum);
      return IOERR;
    } else {
      //read what we can
      buf = current_val.substr(off, size);
      lc->release(inum);
      return OK;
    }
  } else {
    lc->release(inum);
    return IOERR;
  }
}

std::string yfs_client::createBuffElement(yfs_client::inum inum, const char* buf) {
  std::stringstream stream;

  stream << inum;
  stream << INUMSEPERATOR;
  stream << buf;

  return stream.str();
}

 // string split code adapted from: http://stackoverflow.com/questions/236129/splitting-a-string-in-c
void yfs_client::split(const std::string &s, char delim, std::list<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
}

std::list<yfs_client::dirent*>* yfs_client::parsebuf(std::string buf) {
  std::list<yfs_client::dirent*>* entries = new std::list<yfs_client::dirent*>();
  std::list<std::string> split_string;

  split(buf, ELEMENTSEPERATOR, split_string);

  for (std::list<std::string>::iterator it = split_string.begin();
       it != split_string.end();
       it++) {
    entries->push_back(parseDirent(*it));
  }

  return entries;
}

yfs_client::dirent* yfs_client::parseDirent(std::string value) {
  std::list<std::string> split_string;

  split(value, INUMSEPERATOR, split_string);
  std::list<std::string>::iterator it = split_string.begin();

  dirent* entry = new dirent();

  entry->inum = n2i(*it);

  it++;

  entry->name = *it;

  return entry;
}

std::string yfs_client::writeDirent(std::list<yfs_client::dirent*>* entries) {
  std::stringstream stream;
  bool firstLoop = true;

  for (std::list<yfs_client::dirent*>::iterator it = entries->begin();
     it != entries->end();
     it++) {

    if (!firstLoop)
      stream << ELEMENTSEPERATOR;

    stream << createBuffElement((*it)->inum, (*it)->name.c_str());
    firstLoop = false;
  }

  return stream.str();
}

lock_releaser::lock_releaser(extent_client* _ec) {
  ec = _ec;
}

void lock_releaser::dorelease(lock_protocol::lockid_t lid) {
  ec->flush(lid);
}
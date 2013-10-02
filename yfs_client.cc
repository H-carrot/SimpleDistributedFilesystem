// yfs client.  implements FS operations using extent and lock server
#include "extent_client.h"
#include "StringTokenizer.h"
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

  return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
  int r = OK;

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
  return r;
}

int yfs_client::createFile(yfs_client::inum &inum, yfs_client::inum parent, const char* buf) {
  std::string parent_buf;

  printf("\n\nCreating file, parent num: %llu, buf: %s.\n\n", parent, buf);
  setbuf(stdout, NULL);
  // verify that the parent directory actually exists
  if (ec->get(parent, parent_buf) != extent_protocol::OK) {
      printf("\n\nParent doesnt exist\n\n");
      return NOENT;
  }

  printf("\n\nParent exists...");
  // ok the parent exists
  std::list<yfs_client::dirent*>* contents = parsebuf(parent_buf);

  // check for duplicate names
  for (std::list<yfs_client::dirent*>::iterator it = contents->begin();
       it != contents->end();
       it++) {
    if ((*it)->name.compare(buf) == 0 && isfile((*it)->inum)) {
      printf("\n\nError file exists.\n\n");
      return EEXIST;
    }
  }

  printf("\n\nDone searching\n\n");

  // ok there are no files with the same name in this directory
  // we can go ahead and append our new file now to the parent
  // add the seperator if there are multiple items in here
  if (contents->size() !=0 )
    parent_buf += ELEMENTSEPERATOR;

  // generate an inum for our new file
  inum = random();
  inum = inum | 0x80000000; // set the 31 bit correctly

  parent_buf.append(createBuffElement(inum, buf));

  printf("\n\nNew buf:");
  printf(parent_buf.c_str());
  printf("\n\n");

  ec->put(inum, "");
  ec->put(parent, parent_buf);

  return OK;
}

void yfs_client::lookupResource(yfs_client::inum &inum, yfs_client::inum parent, const char* buf) {
  std::string parent_buf;
  bool found = false;
  inum = 0;

  printf("\n\nLooking up file, parent num: %llu.\n\n", parent);

  // verify that the parent directory actually exists
  if (ec->get(parent, parent_buf) != extent_protocol::OK) {
      printf("\n\nParent doesnt exist\n\n");
      return;
  }

  // ok the parent exists
  std::list<yfs_client::dirent*>* contents = parsebuf(parent_buf);

  // scan for the file, and grab the inum if we have it
  for (std::list<yfs_client::dirent*>::iterator it = contents->begin();
       it != contents->end();
       it++) {
    if ((*it)->name.compare(buf) == 0) {
      found = true;
      inum = (*it)->inum;
      break;
    }
  }

  if (found)
    printf("\n\nFound");
  else
    printf("Not Found");

  //return found ? OK : NOENT;
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

  printf("\n\nAttempting parse: %s\n\n", buf.c_str());

  printf("\n Starting split...\n");

  split(buf, ELEMENTSEPERATOR, split_string);

  for (std::list<std::string>::iterator it = split_string.begin();
       it != split_string.end();
       it++) {
    printf("\nParsed line: %s\n", (*it).c_str());
    entries->push_back(parseDirent(*it));
  }

  printf("\nDone parsing...\n");

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

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

  printf("\n\nCreating file.\n\n");

  // verify that the parent directory actually exists
  if (ec->get(inum, parent_buf) != extent_protocol::OK)
    return NOENT;

  // ok the parent exists
  std::list<yfs_client::dirent*>* contents = parsebuf(parent_buf);

  // check for duplicate names
  for (std::list<yfs_client::dirent*>::iterator it = contents->begin();
       it != contents->end();
       it++) {
    if ((*it)->name.compare(buf) == 0 && isfile((*it)->inum))
      return IOERR;
  }

  // ok there are no files with the same name in this directory
  // we can go ahead and append our new file now to the parent
  // add the seperator if there are multiple items in here
  if (contents->size() !=0 )
    parent_buf.append(ELEMENTSEPERATOR);

  parent_buf.append(createBuffElement(inum, buf));

  // generate an inum for our new file
  inum = random();
  inum = inum | 0x80000000; // set thhe 31 bit correctly

  ec->put(inum, "");
  ec->put(parent, parent_buf);

  return OK;
}

std::string yfs_client::createBuffElement(yfs_client::inum inum, const char* buf) {
  std::stringstream stream;

  stream << inum;
  stream << INUMSEPERATOR;
  stream << buf;

  return stream.str();
}

std::list<yfs_client::dirent*>* yfs_client::parsebuf(std::string buf) {
  std::list<yfs_client::dirent*>* entries = new std::list<yfs_client::dirent*>();

  std::string delimiter = ELEMENTSEPERATOR;
  StringTokenizer strtok(buf, delimiter);

  while (strtok.hasMoreTokens())
    entries->push_back(parseDirent(strtok.nextToken()));

  return entries;
}

yfs_client::dirent* yfs_client::parseDirent(std::string value) {
  std::string delimiter = INUMSEPERATOR;
  StringTokenizer strtok(value, delimiter);

  dirent* entry = new dirent();

  entry->inum = n2i(strtok.nextToken());
  entry->name = strtok.nextToken();

  return entry;
}
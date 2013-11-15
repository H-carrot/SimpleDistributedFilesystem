#ifndef yfs_client_h
#define yfs_client_h

#include <list>
#include <string>
#include <vector>

//#include "yfs_protocol.h"
#include "extent_client.h"
#include "lock_client_cache.h"
#include "lock_protocol.h"

// ELEMENTSEPERATOR - what seperates elements in a inode, eg /sub_dir1/sub_dir2/file1/file2
// INUMSEPERATOR    - what seperates an elements name from its inum, eg inum@elementname
// So an example of a directory listing would be: 1234@subdirectory/1235@filename/
#define ELEMENTSEPERATOR '/'
#define INUMSEPERATOR    '@'

class lock_releaser : public lock_release_user {
public:
  lock_releaser(extent_client*);
  virtual ~lock_releaser() {};
  void dorelease(lock_protocol::lockid_t);

private:
  extent_client* ec;
};

class yfs_client {
  extent_client       *ec;
  lock_client_cache   *lc;
  lock_releaser       *lu;
 public:

  typedef unsigned long long inum;
  enum xxstatus { OK, RPCERR, NOENT, IOERR, EXIST };
  typedef int status;

  struct fileinfo {
    unsigned long long size;
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirinfo {
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirent {
    std::string name;
    yfs_client::inum inum;
  };

 private:
  static inum n2i(std::string);
  static std::string filename(inum);
  static void split(const std::string &, char, std::list<std::string> &);
 public:

  yfs_client(std::string, std::string);

  bool isfile(inum);
  bool isdir(inum);

  int getfile(inum, fileinfo &);
  int getdir(inum, dirinfo &);

  int createFile(inum &, inum, const char*);
  int createDirectory(inum &, inum, const char*);
  int unlinkFile(inum, const char*);
  std::string writeDirent(std::list<yfs_client::dirent*>*);
  int lookupResource(yfs_client::inum &, yfs_client::inum, const char*);
  int getDirContents(yfs_client::inum, std::string &);
  int setAttr(yfs_client::inum, yfs_client::fileinfo);
  int writeFile(yfs_client::inum, const char*, size_t, off_t);
  int readFile(yfs_client::inum, std::string &, size_t, off_t);

  std::string createBuffElement(yfs_client::inum, const char*);
  std::list<yfs_client::dirent*>* parsebuf(std::string);
  dirent* parseDirent(std::string);
};

#endif

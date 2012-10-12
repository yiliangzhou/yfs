#ifndef yfs_client_h
#define yfs_client_h

#include <string>
//#include "yfs_protocol.h"
#include "extent_client.h"
#include "lock_client.h"
#include <vector>

#include "lock_protocol.h"
#include "lock_client.h"

class yfs_client {
  extent_client *ec;
  lock_client *lc;
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
  static std::string filename(inum);
  static inum n2i(std::string);
  std::vector<dirent> parse_dirents(const std::string &buf); 
  void load_root(extent_client *);
 public:

  yfs_client(std::string, std::string);

  bool isfile(inum);
  bool isdir(inum);

  // to determined if any file name already exist. 
  bool exist(inum, const char*, inum &);
  int create(inum, const char*, inum &);
  int read_dirents(inum, std::vector<dirent>&);
  int set_attr_size(inum, unsigned int);
  int read(inum, size_t, off_t, std::string &);
  int write(inum, size_t, off_t, const std::string &);
 
  int getfile(inum, fileinfo &);
  int getdir(inum, dirinfo &);
};

#endif 

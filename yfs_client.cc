// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <stdlib.h>

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
  ec = new extent_client(extent_dst);
  //  lc = new lock_client(lock_dst);
  srand((unsigned)time(NULL));
  // unsigned long long N = 4294967296;
  load_root(ec); 
}

void yfs_client::load_root(extent_client *ec) {
  // check if directory 0x00000001 exist
  inum inum = 0x00000001;
  dirinfo din;
  
  // if not exist, create one
  if(getdir(inum, din) != OK) {
    std::string buf;      // empty directory
    ec->put(inum, buf);
    std::cout<<"create root\n";
  }

  // otherwise, do nothing and return 
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

std::vector<yfs_client::dirent> yfs_client::parse_dirents(const std::string &buf) {

  std::vector<dirent> ents;
  if(buf.size() == 0) { return ents; }

  std::istringstream ss(buf);
  std::string str_entry;
  while(std::getline(ss, str_entry, ';')) {
    dirent entry;
    entry.name = str_entry.substr(0, str_entry.find(',') );
    entry.inum = n2i(str_entry.substr( str_entry.find(',')+1));
    ents.push_back(entry);
  }
  return ents;
}

int 
yfs_client::read_dirents(inum directory_inum, std::vector<dirent> &ents) {
  int r = OK;
  std::string buf;
  if(extent_protocol::OK != ec->get(directory_inum, buf)) {
    return IOERR;
  }

  ents = parse_dirents(buf);
  return r;
}

bool
yfs_client::exist(inum parent_inum, const char* name, inum & inum) {

// retrieve content of parent_inum, search if name appears
  std::string buf;
  if(extent_protocol::OK != ec->get(parent_inum, buf)) {
    return false;
  }
  std::vector<dirent> ents = parse_dirents(buf);
   
  for(std::vector<dirent>::iterator it = ents.begin(); it != ents.end(); it++) {
    if(0 == (*it).name.compare(name)) {
      inum = (*it).inum; 
      return true;
    }
  }
     
  return false;
}

int
yfs_client::create(inum parent_inum, const char* name, inum &inum)
{
  int r = OK;

  // Check if a file with the same name already exist
  // under parent directory  
  if(exist(parent_inum, name, inum)) { 
    r = EXIST; 
    // goto release;
    return r;
  }
  
   // Pick up an ino for file name set the 32nd bit to 1
  unsigned long long rnd = rand();
  inum = rnd | 0x80000000; // set the 32nd bit to 1

  // Create an empty extent for ino. 
  std::string empty_str;
  if (ec->put(inum, empty_str) != extent_protocol::OK) {
    r = IOERR;
    return r;
  }

  // Update parent directory's buf in extent server
  std::string buf;
  ec->get(parent_inum, buf);
  buf.append(name);
  buf.append(",");
  buf.append(filename(inum));
  buf.append(";");
  if(ec->put(parent_inum, buf) != extent_protocol::OK) {
    r = IOERR;
    // goto release;
    return r;
  }
  
  return r;
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




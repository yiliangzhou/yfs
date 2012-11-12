// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include "lock_client.h"
#include "lock_client_cache.h"
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
  // to guard the extent server from breaking by concurrent operations 
  // lc = new lock_client(lock_dst);

  // ec is a lock_release_user
  lc = new lock_client_cache(lock_dst, ec);  
  // use random seed
  srand((unsigned)time(NULL));

  // make sure root directory, with inum = 0x00000001,
  // is ready to be queried.
  load_root(ec); 
}

void yfs_client::load_root(extent_client *ec) {
  inum inum = 0x00000001;
  dirinfo din;
  scoped_lock_ lock(lc, inum);
  // check if directory 0x00000001 exist
  // if not exist, create one
  if(getdir_(inum, din) != OK) {
    std::string buf;      // empty directory
    ec->put(inum, buf);
  }

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


std::string yfs_client::dirents_to_string(std::vector<yfs_client::dirent> & ents) {
  std::ostringstream os;
  
  std::vector<dirent>::iterator it = ents.begin();
  while(it != ents.end()){
    os <<(*it).name<<","<<(*it).inum<<";";
    it++;
  }
  
  return os.str();
}

int
yfs_client::set_attr_size(inum file_inum, unsigned int new_size) {
  int r = yfs_client::OK;
  
  // request a lock on file_inum
  // lc->acquire(file_inum);
  scoped_lock_ lock(lc, file_inum);

  // get old attr and content
  extent_protocol::attr a;
  if(extent_protocol::OK != ec->getattr(file_inum, a)) {
    // lc->release(file_inum);
    return IOERR;
  }
  unsigned int old_size = a.size;

  std::string buf;
  if(extent_protocol::OK != ec->get(file_inum, buf)) {
    // lc->release(file_inum);
    return IOERR;
  }

  if(new_size >  old_size) {
    // pading at end of buf with '\0'
    buf.resize(new_size, '\0');
  }else if(new_size < old_size) {
    // truncate buf to new size
    buf.resize(new_size);
  }else {
    // do nothing
  }
  
  if(extent_protocol::OK != ec->put(file_inum, buf)) {
    // lc->release(file_inum);
    return IOERR;
  } 
  
  // lc->release(file_inum);
  return r;
} 

int
yfs_client::write(inum file_inum, size_t size, off_t offset, const std::string &buf) {
  int r = OK;
  scoped_lock_ lock(lc, file_inum); 
  // get old data before write 
  std::string old_data;
  if(extent_protocol::OK != ec->get(file_inum, old_data)) {
    return IOERR;
  }

  std::string new_data;  
  if(offset < old_data.size()) {
    // overwrite old data.
    new_data = old_data.substr(0, offset);
    new_data.append(buf);
    
    // in case offset + size < old_data.size()
    if(offset + size < old_data.size()) {
      // append the tail of old data to new data.
      new_data.append(old_data.substr(offset + size));
    }
  }else{ 
    // otherwise append null '\0' character to fill the gap, could be zero
    // when offset = tmp_buf.size()
    new_data = old_data;
    new_data.append(offset-old_data.size(), '\0');
    new_data.append(buf);
  }

  if(extent_protocol::OK != ec->put(file_inum, new_data)) {
    return IOERR;
  }

  return r;
}


int
yfs_client::read(inum file_inum, size_t size, off_t offset, std::string &buf) {
  int r = OK;
  scoped_lock_ lock(lc, file_inum);
 
  std::string tmp_buf;
  if(extent_protocol::OK != ec->get(file_inum, tmp_buf)) {
    return IOERR;
  }
  
  // TODO: should I consider this situation?
  // Should i assume this duty here.
  if(offset >= tmp_buf.size()) {
    return OK;
  }

  // fetch requested sub string based on size and offset  
  if(offset + size > tmp_buf.size()) {
    buf = tmp_buf.substr(offset);
  }else{
    buf = tmp_buf.substr(offset, size);
  }
  
  return r;
}


// unlocked-version read_dirents()
int 
yfs_client::read_dirents_(inum directory_inum, std::vector<dirent> &ents) {
  int r = OK;
  std::string buf;
  if(extent_protocol::OK != ec->get(directory_inum, buf)) {
    return IOERR;
  }

  ents = parse_dirents(buf);
  return r;
}


int 
yfs_client::read_dirents(inum directory_inum, std::vector<dirent> &ents) {
  int r = OK;
  scoped_lock_ lock(lc, directory_inum);
  std::string buf;
  if(extent_protocol::OK != ec->get(directory_inum, buf)) {
    return IOERR;
  }

  ents = parse_dirents(buf);
  return r;
}


// unlocked-version exist()
bool
yfs_client::exist_(inum parent_inum, const char* name, inum & inum) {
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
 

bool
yfs_client::exist(inum parent_inum, const char* name, inum & inum) {

  scoped_lock_ lock(lc, parent_inum);
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
yfs_client::create(inum parent_inum, const char* name, inum &inum, int file_or_dir)
{
  int r = OK;

  // Check if a file with the same name already exist
  // under parent directory  
  if(exist_(parent_inum, name, inum)) { 
    r = EXIST; 
    return r;
  }
   
  // Pick up an ino for new file/dir
  unsigned long long rnd = rand();
  switch (file_or_dir) {
    case FILE_:
      inum = rnd | 0x80000000; // set the 32nd bit to 1
      break;
    case DIR_:
      inum = rnd & ~ (1<<31); // set the 32nd bit to 0
      break;
    default:
      inum = rnd | (1<<31);
      break;         
  }

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
    return r;
  }
  
  return r;
}


// this will create an empty file  under directory indicated
// by parent_inum.
// will return EXIST if a file or directory with the same name
// already exist.
int
yfs_client::create_file(inum parent_inum, const char* name, inum & inum) {
  int r = OK;
  scoped_lock_ lock(lc, parent_inum);
  r = create(parent_inum, name, inum, FILE_);
  return r;
}

// this will create a empty directory under directory indicated
// by parent_inum.
// will return EXIST if a file or directory with the same name
// already exist.
int
yfs_client::mkdir(inum parent_inum, const char* name, inum & inum) {
  int r = OK;
  scoped_lock_ lock(lc, parent_inum);
  std::cout<<"mkdir #1"<<std::endl;
  r = create(parent_inum, name, inum, DIR_);
  std::cout<<"mkdir #2"<<std::endl;
  return r;
}

int
yfs_client::unlink(inum parent_inum, const char* name)
{
  int r = OK;
  scoped_lock_ lock(lc, parent_inum);
  // Check if this file does exist.
  inum inum;
  if(exist_(parent_inum, name, inum) && isfile(inum)) { 
    // file exist
    scoped_lock_ file_lock(lc, inum);
    // 1. remove a dirent from the directory
    std::vector<dirent> ents;
    read_dirents_(parent_inum, ents);
    std::vector<dirent>::iterator it = ents.begin();
    while(it != ents.end()) {
      if((*it).inum == inum) { ents.erase(it); break; }
      // otherwise increase the it
      it++;
    } // updated ents 
    std::string buf = dirents_to_string(ents);
    // write back new extent of the directory
    if(ec->put(parent_inum, buf) != extent_protocol::OK) {
      return IOERR;
    }

    // 2. delete extent of a file
    if(ec->remove(inum) != extent_protocol::OK) {
      return IOERR;
    }

  }else{
    // no entry with the name is found under directory,
    // or a directory entry is found under directory.
    // we are not allowed to unlink a directory. 
    return NOENT; 
  }
  
  return r;
}


int
yfs_client::getfile(inum inum, fileinfo &fin)
{
  int r = OK;
  // You modify this function for Lab 3
  // - hold and release the file lock
  scoped_lock_ lock(lc, inum);
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


// unlocked-version getdir()
int
yfs_client::getdir_(inum inum, dirinfo &din)
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

int
yfs_client::getdir(inum inum, dirinfo &din)
{
  int r = OK;
  // You modify this function for Lab 3
  // - hold and release the directory lock
  scoped_lock_ lock(lc, inum);

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



// the extent server implementation

#include "extent_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <rpc/slock.h>
#include <new>
/*
    TODO: try this later
    #include <memory>
    Try std::tr1::shared_ptr in this implementation
    Bear in mind that std::auto_ptr can't be used in STL container
*/

extent_server::extent_server() {
    // global mutex
    pthread_mutex_init(&mutex_g, NULL);
}

extent_server::~extent_server() {
    // will std::map free its data correctly?
    pthread_mutex_destroy(&mutex_g);	
    for(ExtentMap::iterator it = id_2_content.begin(); it != id_2_content.end();
	    it++) {
        delete it->second;
    }
}

int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &)
{
  int ret = extent_protocol::OK;

  ScopedLock ml(&mutex_g);

  // set attribute ctime and mtime to time()
  unsigned int seconds = (unsigned int)time(NULL);
  extent_protocol::attr attribute;
  attribute.mtime = seconds;
  attribute.ctime = seconds;
  attribute.size = buf.size();
  // atime should not be set here..
  // attribute.atime = 0;

  // a new or an old extent
  if(id_2_content.find(id) == id_2_content.end()) {
     Content *ptr_content = new Content(buf, attribute);
     id_2_content.insert(ExtentMap::value_type(id, ptr_content));
  }else{
    // an old extent being updated
    Content &content = * id_2_content[id];

    content.buf_ = buf;
    content.attribute_.mtime = seconds;
    content.attribute_.ctime = seconds;
    content.attribute_.size = buf.size();
    // only atime is not updated
  }

  return ret;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf)
{ 
  ScopedLock ml(&mutex_g);
  int ret = extent_protocol::OK;
  
  if(id_2_content.find(id) == id_2_content.end()) {
    ret = extent_protocol::NOENT;
  }else{
    buf = id_2_content[id]->buf_;
    // set attribute atime to time()
    id_2_content[id]->attribute_.atime = (unsigned int)time(NULL);
  } 
 
  return ret;
}

int extent_server::getattr(extent_protocol::extentid_t id, extent_protocol::attr &a)
{
  // You fill this in for Lab 2.
  // You replace this with a real implementation. We send a phony response
  // for now because it's difficult to get FUSE to do anything (including
  // unmount) if getattr fails.
  ScopedLock ml(&mutex_g);
  int ret = extent_protocol::OK;
  
  if(id_2_content.find(id) == id_2_content.end()) {
    ret = extent_protocol::NOENT;
  }else{
    extent_protocol::attr attr = id_2_content[id]->attribute_; 
    a.size = attr.size;
    a.atime = attr.atime;
    a.mtime = attr.mtime;
    a.ctime = attr.ctime;
  }
  return ret;
}

int extent_server::remove(extent_protocol::extentid_t id, int &)
{
  ScopedLock ml(&mutex_g);
  int ret = extent_protocol::OK;
  
  ExtentMap::iterator it = id_2_content.find(id);
  if(it == id_2_content.end()) {
    ret = extent_protocol::NOENT;
  }else{
    // release memory
    id_2_content.erase(it);
    // delete id_2_content[id];
    // id_2_content.erase(id);
  }

  return ret;
}

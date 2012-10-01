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
    Bear in mind that std::auto_ptr
*/

extent_server::extent_server() {
    // pthread_mutex_init(&mutex_id_2_content, NULL);
	// global mutex
    pthread_mutex_init(&mutex_g, NULL);
}

/*
    TODO: Understand virtual destructor
*/
extent_server::~extent_server() {
    // TODO: answer the following question
    // will std::map free its data correctly?
    // pthread_mutex_destory(&mutex_id_2_content);
    pthread_mutex_destroy(&mutex_g);	
    for(ExtentMap::iterator it = id_2_content.begin(); it != id_2_content.end();
	    it++) {
        delete it->second;
    }
}

int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &)
{
  int ret = extent_protocol::OK;
  //  pthread_mutex_lock(&mutex_id_2_content);    

  ScopedLock ml(&mutex_g);

  // TODO: understand reference and pointer in c++
  // TODO: set attribute ctime and mtime to time()
  unsigned int seconds = (unsigned int)time(NULL);
  extent_protocol::attr attribute;
  attribute.mtime = seconds;
  attribute.ctime = seconds;
  attribute.size = buf.size();
  // Initilization. I'm not sure if this is correct.
  attribute.atime = 0;

  // a new file or an old extent
  if(id_2_content.find(id) == id_2_content.end()) {
     std::cout<<"ID:"<<id<<std::endl;
     std::cout<<"In extent server,new file's content is:"<<buf<<std::endl;
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
  // TODO: what if i write string str = "hello";  buf = str; 
  ScopedLock ml(&mutex_g);
  int ret = extent_protocol::OK;
  
  if(id_2_content.find(id) == id_2_content.end()) {
    ret = extent_protocol::NOENT;
  }else{
    // what's the behavior of default assignment
    buf = id_2_content[id]->buf_;
    // TODO: set attribute atime to time()
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
  // TODO: Does std::map support concurrent read??
  
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

  if(id_2_content.find(id) == id_2_content.end()) {
    ret = extent_protocol::IOERR;
  }else{
    // release memory
    delete id_2_content[id];
    id_2_content.erase(id);
  }

  return ret;
}

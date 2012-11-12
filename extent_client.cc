// RPC stubs for clients to talk to extent_server

#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <rpc/slock.h>

// The calls assume that the caller holds a lock on the extent

extent_client::extent_client(std::string dst)
{
  sockaddr_in dstsock;
  make_sockaddr(dst.c_str(), &dstsock);
  cl = new rpcc(dstsock);
  if (cl->bind() != 0) {
    printf("extent_client: bind failed\n");
  }
  
  pthread_mutex_init(&local_cache_lock, NULL);
}

extent_client::~extent_client() {
  pthread_mutex_destroy(&local_cache_lock);
  for(ExtentMap::iterator it = id_2_content.begin(); it != id_2_content.end();
      it++) {
    delete it->second;
  }
}

extent_protocol::status
extent_client::get(extent_protocol::extentid_t eid, std::string &buf)
{
  extent_protocol::status ret = extent_protocol::OK;
    
  ScopedLock ml(&local_cache_lock);  

  if( id_2_content.find(eid) == id_2_content.end() ) { 
    // fetch content and attribute from extent server
    std::string tmp_buf;
    extent_protocol::attr tmp_attr;
    ret = cl->call(extent_protocol::get, eid, tmp_buf);
    ret = cl->call(extent_protocol::getattr, eid, tmp_attr);
    
    // add cached record for current eid
    Content *p_cached_content = new Content(tmp_buf, tmp_attr);
    id_2_content.insert(ExtentMap::value_type(eid, p_cached_content)); 
  }  

  if(id_2_content[eid]->dirty == REMOVED) {
    // return error
  }  

  // read from cache  
  buf = id_2_content[eid]->buf_;
  id_2_content[eid]->attribute_.atime = (unsigned int)time(NULL); 

  return ret;
}

extent_protocol::status
extent_client::getattr(extent_protocol::extentid_t eid, 
		       extent_protocol::attr &attr)
{
  extent_protocol::status ret = extent_protocol::OK;
  // ret = cl->call(extent_protocol::getattr, eid, attr);
  ScopedLock ml(&local_cache_lock);
  
  if(id_2_content.find(eid) == id_2_content.end()) {
    std::string tmp_buf;
    extent_protocol::attr tmp_attr;
    ret = cl->call(extent_protocol::get, eid, tmp_buf);
    ret = cl->call(extent_protocol::getattr, eid, tmp_attr);
    
    // add cached record for current eid
    Content *p_cached_content = new Content(tmp_buf, tmp_attr);
    id_2_content.insert(ExtentMap::value_type(eid, p_cached_content)); 
//    ret = cl->call(extent_protocol::getattr, eid, attr);
    // return ret;
  } 

  extent_protocol::attr cached_attr = id_2_content[eid]->attribute_; 
  attr.size = cached_attr.size;
  attr.atime = cached_attr.atime;
  attr.mtime = cached_attr.mtime;
  attr.ctime = cached_attr.ctime;
 
  return ret;
}

extent_protocol::status
extent_client::put(extent_protocol::extentid_t eid, std::string buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  
  ScopedLock ml(&local_cache_lock); 

  unsigned int seconds = (unsigned int)time(NULL);

  if ( id_2_content.find(eid) == id_2_content.end() ) {
    // not found in cache, build a new cached record for eid
    extent_protocol::attr attribute;
    attribute.mtime = seconds;
    attribute.ctime = seconds;
    attribute.size = buf.size();

    Content *p_cached_content = new Content(buf, attribute);
    id_2_content.insert(ExtentMap::value_type(eid, p_cached_content));
  }else{ 
    // found cached record for current eid
    // could be a removed one. whatever. 
    Content &content = * id_2_content[eid];
    
    content.buf_ = buf;
    content.attribute_.mtime = seconds;
    content.attribute_.ctime = seconds;
    content.attribute_.size = buf.size();

    // what if we hit a removed cached
  }
 
  // could be 1. clean --> modified
  //          2. removed --> modified
  id_2_content[eid]->dirty = MODIFIED;
 
  return ret;
}

extent_protocol::status
extent_client::remove(extent_protocol::extentid_t eid)
{
  extent_protocol::status ret = extent_protocol::OK;
   
  ScopedLock ml(&local_cache_lock);  
/*  
  if (id_2_content.find(eid) == id_2_content.end() ) {
    int r;
    ret = cl->call(extent_protocol::remove, eid, r); 
    return ret;
  }
*/
  // for not cached eid, create a new record in cache 
  if(id_2_content.find(eid) == id_2_content.end()) {
    std::string empty_str("");
    extent_protocol::attr empty_attr;

    id_2_content[eid] = new Content(empty_str, empty_attr);
  }

  // flag removed in cache
  id_2_content[eid]->dirty = REMOVED;

  return ret;
}

void extent_client::flush(extent_protocol::extentid_t eid) { 

  ScopedLock ml(&local_cache_lock);  
  int r;
  // write back if dirty, or not clean
  switch (id_2_content[eid]->dirty) {
    case MODIFIED:
      // write back
      cl->call(extent_protocol::put, eid, id_2_content[eid]->buf_, r); 
      break;
    case REMOVED:
      cl->call(extent_protocol::remove, eid, r);
      break;
    case CLEAN:
      break;
  }

  // erase the cache
  delete id_2_content[eid];
  id_2_content.erase(eid);
}

void extent_client::dorelease(lock_protocol::lockid_t lid) {
  // assume lid is some eid  
  this->flush(extent_protocol::extentid_t (lid));
}

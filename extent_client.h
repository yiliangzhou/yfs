// extent client interface.

#ifndef extent_client_h
#define extent_client_h

#include <string>
#include <map>
#include "extent_protocol.h"
#include "rpc.h"
#include "lock_client_cache.h"
#include <pthread.h>

class extent_client : public lock_release_user {
 private:
  rpcc *cl;

  // mutex to protect our local cache
  pthread_mutex_t local_cache_lock;
  enum Dirty { CLEAN, MODIFIED, REMOVED };
  // local cache for an extent_client instance
  typedef struct Content{
    std::string buf_;
    extent_protocol::attr attribute_;
    Dirty dirty;

    Content(std::string buf, extent_protocol::attr attribute) : 
       buf_(buf), attribute_(attribute), dirty(CLEAN) {}
  }Content, *pContent;

  typedef std::map<extent_protocol::extentid_t, pContent> ExtentMap;
  // data structure to store mapping from id to string and attributes
  ExtentMap id_2_content;
 
  void flush(extent_protocol::extentid_t eid);

 public:
  extent_client(std::string dst);
  ~extent_client();

  extent_protocol::status get(extent_protocol::extentid_t eid, 
			      std::string &buf);
  extent_protocol::status getattr(extent_protocol::extentid_t eid, 
				  extent_protocol::attr &a);
  extent_protocol::status put(extent_protocol::extentid_t eid, std::string buf);
  extent_protocol::status remove(extent_protocol::extentid_t eid);
  
  virtual void dorelease(lock_protocol::lockid_t lid);
};

#endif 


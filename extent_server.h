// this is the extent server

#ifndef extent_server_h
#define extent_server_h

#include <string>
#include <map>
#include <set>
#include "extent_protocol.h"
#include <pthread.h>
// TODO: what's the difference between <> and ""
// TODO: what's the difference between pointer * and reference &

class extent_server {
 private:
  typedef struct Content{
   std::string buf_;
   extent_protocol::attr attribute_;

   // TODO: add the struct's constructor and destructor
   Content(std::string buf, extent_protocol::attr attribute) : 
       buf_(buf), attribute_(attribute) {}
 
  }Content, *pContent;

  typedef std::map<extent_protocol::extentid_t, pContent> ExtentMap;
  // data structure to store mapping from id to string and attributes
  ExtentMap id_2_content;
 
  typedef struct DirEntry {
    std::string name;
	extent_protocol::extentid_t id;
  }DirEntry, *pDirEntry;
  // data structure to store mapping from dirctory_id to entries
  typedef std::set<pDirEntry> DirEntries, *pDirEntries;
  typedef std::map<extent_protocol::extentid_t, DirEntries> DirMap;
  DirMap id_2_dir_entries;
  // mutex to protect access to id_2_content
  // pthread_mutex_t mutex_id_2_content;
  pthread_mutex_t mutex_g;
  // pthread_cond_t cond_id_2_content;

 public:
  extent_server();
  ~extent_server();

  int put(extent_protocol::extentid_t id, std::string, int &);
  int get(extent_protocol::extentid_t id, std::string &);
  int getattr(extent_protocol::extentid_t id, extent_protocol::attr &);
  int remove(extent_protocol::extentid_t id, int &);
};

#endif 








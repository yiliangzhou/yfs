#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>

#include <map>
// FIFO queue
#include <queue>
#include "pthread.h"

#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"

class lock_server_cache {
 private:
  int nacquire;
  enum LockState {
    FREE, NOT_AVAILABLE };
  typedef struct lock_state_ {
    LockState ls;
    std::string owner;    
    std::queue<std::string> requests;
     
    lock_state_(): ls(FREE), owner("") {};
  } lock_state;

  std::map<lock_protocol::lockid_t, lock_state> l_state;
  pthread_mutex_t m;

 public:
  lock_server_cache();
  ~lock_server_cache();
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  int acquire(lock_protocol::lockid_t, std::string id, int &);
  int release(lock_protocol::lockid_t, std::string id, int &);
};

#endif

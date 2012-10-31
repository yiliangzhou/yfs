// lock client interface.

#ifndef lock_client_cache_h

#define lock_client_cache_h

#include <string>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_client.h"
#include "lang/verify.h"

// for exclusive access to the cached locks
#include "pthread.h"
// use map to store the cache
#include <map>
#include <vector>

// Classes that inherit lock_release_user can override dorelease so that 
// that they will be called when lock_client releases a lock.
// You will not need to do anything with this class until Lab 5.
class lock_release_user {
 public:
  virtual void dorelease(lock_protocol::lockid_t) = 0;
  virtual ~lock_release_user() {};
};

class lock_client_cache : public lock_client {
 private:
  class lock_release_user *lu;
  int rlock_port;
  std::string hostname;
  std::string id;
  
  // mutex for controlling exclusive access cache
  // condition variable for availablity of the lock
  pthread_mutex_t m;

  // each cached lock can have the following state
  // in client side.
  enum LockState { NONE, 
                   FREE, 
                   LOCKED, 
                   ACQUIRING, 
                   RELEASING };

  typedef struct lock_state_ {
    LockState ls;
    bool is_revoked;
    bool retry;
    int revoke_count;
    pthread_cond_t retry_cond;
    pthread_cond_t avail_cond;
    int wait_count; 
    
    lock_state_(): ls(NONE),
                  is_revoked(false),
                  retry(false),
                  wait_count(0),
                  revoke_count(0) {
      pthread_cond_init(&retry_cond, NULL);
      pthread_cond_init(&avail_cond, NULL);
    }

    ~lock_state_() {
      pthread_cond_destroy(&retry_cond);
      pthread_cond_destroy(&avail_cond);
    }

  } lock_state, *ptr_lock_state;


  // lock caches
  std::map<lock_protocol::lockid_t, ptr_lock_state> cache; 
  
 public:
  lock_client_cache(std::string xdst, class lock_release_user *l = 0);
  virtual ~lock_client_cache();
  lock_protocol::status acquire(lock_protocol::lockid_t);
  lock_protocol::status release(lock_protocol::lockid_t);
  rlock_protocol::status revoke_handler(lock_protocol::lockid_t, 
                                        int &);
  rlock_protocol::status retry_handler(lock_protocol::lockid_t, 
                                       int &);
// inherited fields
// rpcc *cl;

};


#endif

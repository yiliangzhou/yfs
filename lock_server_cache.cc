// the caching lock server implementation

#include "lock_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"


lock_server_cache::lock_server_cache()
{
  pthread_mutex_init(&m, NULL);
}

lock_server_cache::~lock_server_cache() {
  pthread_mutex_destroy(&m);
}

int lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id, 
                               int &)
{
  lock_protocol::status ret = lock_protocol::OK;
  pthread_mutex_lock(&m);
  switch(l_state[lid].ls) {
    case FREE:
      // grant imediately
      l_state[lid].owner = id;
      l_state[lid].ls = NOT_AVAILABLE;
      pthread_mutex_unlock(&m); 
      break;
    case NOT_AVAILABLE:
      bool send_revoke = false;
      if(l_state[lid].requests.empty()) {
        send_revoke = true;
      }
      l_state[lid].requests.push(id);
      ret = lock_protocol::RETRY;
      // revoke the owner if the queue is empty
      if(send_revoke) {
        // printf("------owner is -----%s tid: %u\n",l_state[lid].owner.c_str(), pthread_self());
        std::string owner = l_state[lid].owner;

        pthread_mutex_unlock(&m);
        handle h(l_state[lid].owner);
        rpcc *cl = h.safebind();
        int r;
        VERIFY ( cl != NULL);
        // printf("call revoke of %u in tid: %u\n",lid, pthread_self());
        cl->call(rlock_protocol::revoke, lid, r); 
      }else{
        pthread_mutex_unlock(&m);
      }
      break;
  }
  return ret;
}

int 
lock_server_cache::release(lock_protocol::lockid_t lid, std::string id, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  r = 0;
 
  pthread_mutex_lock(&m);
  VERIFY(id.compare(l_state[lid].owner) == 0);
  if(l_state[lid].ls == FREE) {
    pthread_mutex_unlock(&m);
    return lock_protocol::RPCERR;
  }

  if(!l_state[lid].requests.empty()){
    // grant the frist waiting request the lock
    l_state[lid].owner = l_state[lid].requests.front();
    l_state[lid].requests.pop();
    std::string owner = l_state[lid].owner;
    // new request will see the invariant holds.
    // new revoke will send to this new owner, might arrive
    // before the acquire return OK.
    bool be_revoked = !l_state[lid].requests.empty();
    pthread_mutex_unlock(&m);
    handle h(owner);
    rpcc *cl = h.safebind();
    int r;
    VERIFY (cl != NULL);
    cl->call(rlock_protocol::retry, lid, r); 
    // this will revoke current owner twice possible.
    if(be_revoked) {
      cl->call(rlock_protocol::revoke, lid, r);
    }
    // delete cl;
  }else{
    // no request in the queue
    l_state[lid].ls = FREE;
    l_state[lid].owner = "";  
    pthread_mutex_unlock(&m);
  }

  return ret;
}

lock_protocol::status
lock_server_cache::stat(lock_protocol::lockid_t lid, int &r)
{
  tprintf("stat request\n");
  r = nacquire;
  return lock_protocol::OK;
}


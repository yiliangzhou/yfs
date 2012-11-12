// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include "tprintf.h"


lock_client_cache::lock_client_cache(std::string xdst, 
				     class lock_release_user *_lu)
  : lock_client(xdst), lu(_lu)
{
  rpcs *rlsrpc = new rpcs(0);
  rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache::revoke_handler);
  rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache::retry_handler);

  const char *hname;
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << rlsrpc->port();
  id = host.str();
  
  // init the mutex and cond during construction
  pthread_mutex_init(&m, NULL);
  // pthread_cond_init(&cond, NULL);
}

lock_client_cache::~lock_client_cache() {
  // destroy mutex and cond during destruction
  pthread_mutex_destroy(&m);
  std::map<lock_protocol::lockid_t, lock_state*>::iterator it;
  for(it = cache.begin(); it != cache.end(); it++) {
    delete (*it).second;
  } 
}

lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid)
{
  int r;
  lock_protocol::status ret = lock_protocol::OK; 
  pthread_mutex_lock(&m);

  // only construct once.
  if(cache[lid] == NULL) {
    // printf("new lock record %u created in thread %u\n",lid, pthread_self() );
    cache[lid] = new lock_state();
    VERIFY(cache[lid]->wait_count == 0);
    VERIFY(cache[lid]->ls == NONE);
    VERIFY(cache[lid]->retry == false);
  }
  
  // the lock has the following state:
  // NONE, ACQUIRING, LOCKED, FREE, RELEASING
  
  // printf("acquire lid %u in thread %u before releasing\n", lid, pthread_self());
  while(cache[lid]->ls == RELEASING) {
    pthread_cond_wait(&cache[lid]->avail_cond, &m);
  }
  // printf("waiting lid %u in thread %u after releasing\n", lid, pthread_self());
  // the lock has the following state:
  // NONE, ACQUIRING, LOCKED, FREE

  // acquire either from cache or rpc 
  if(cache[lid]->ls == NONE) {
    // init the structure of this lid 
    // cache[lid]->is_revoked = false;
    // cache[lid]->retry = false;
    // cache[lid]->wait_count = 0; 
    // Know nothing about the lid
    // acquire from server
    cache[lid]->ls = ACQUIRING;
    pthread_mutex_unlock(&m);
    // acquiring the lock from server.
    ret = cl->call(lock_protocol::acquire, lid, id, r);
    // see if the lock is current unavailable
    pthread_mutex_lock(&m);
    if(ret == lock_protocol::RETRY) {
       // wait for the server to notify the availability
       // of the lock by using retry rpc.
       // the handler will wake me up.
       if(!cache[lid]->retry) {
         // printf("waiting server lid %u in thread %u\n", lid, pthread_self());
         pthread_cond_wait(&cache[lid]->retry_cond, &m);
         // printf("gained server lid %u in thread %u\n", lid, pthread_self());
       }
       cache[lid]->retry = false;
    }
    // VERIFY (ret == lock_protocol::OK);
    cache[lid]->ls = LOCKED;
  }else{
    // hit cache
    // the lock has following state:
    // ACQUIRING, LOCKED, FREE
    cache[lid]->wait_count++;
    
    // printf("waiting cached lid %u in thread %u\n", lid, pthread_self());
    while(cache[lid]->ls != FREE) {
      pthread_cond_wait(&cache[lid]->avail_cond, &m);
    }
    // printf("gained cached lid %u in thread %u\n", lid, pthread_self());
    cache[lid]->wait_count--;
    cache[lid]->ls = LOCKED;
  }
  
  // the lock can only has the following state:
  // LOCKED
  pthread_mutex_unlock(&m);
 
  // VERIFY (ret == lock_protocol::OK); 
  return r;
}

lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid)
{
  int r;
  // if no revoke message received,
  // release the lock in cache and notify threads
  // are waiting for this lock.
  pthread_mutex_lock(&m);
  
  VERIFY(cache[lid]->ls == LOCKED);  
  if(cache[lid]->revoke_count > 0 && cache[lid]->wait_count == 0) {
  // if(cache[lid]->is_revoked && cache[lid]->wait_count==0) { 
     cache[lid]->ls = RELEASING;
     cache[lid]->revoke_count--;
     pthread_mutex_unlock(&m);

     std::cout<<"dorelease called in release"<<std::endl;  
     // notify the user, extent_client, the comming lid releasing
     lu->dorelease(lid);

     lock_protocol::status ret = cl->call(lock_protocol::release, lid, id, r);
     VERIFY (ret == lock_protocol::OK); 
     // printf("release to server lid = %u, in thread %u in release \n", lid, pthread_self());
     pthread_mutex_lock(&m);
     // cache[lid]->is_revoked = false;
     cache[lid]->ls = NONE;
  }else{
    // do not return the lock to server
    // printf("the revoke_count for lid %u is %d\n", lid, cache[lid]->revoke_count);
    // printf("current state of lid: %u is %d\n", lid, cache[lid]->ls);
    // printf("release to locally count %d for lid = %u, in thread %u \n",cache[lid]->wait_count, lid, pthread_self());
    cache[lid]->ls = FREE;
  }
  // when release done, the state of the lock changed
  // either from RELEASING to NONE, or LOCKED to FREE 
  pthread_cond_broadcast(&cache[lid]->avail_cond);
  pthread_mutex_unlock(&m);

  return lock_protocol::OK;
}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, 
                                  int &)
{
  int ret = rlock_protocol::OK;
  pthread_mutex_lock(&m);
  // printf("revoke for lid = %u get called in tid %u\n", lid, pthread_self());
  cache[lid]->is_revoked = true;
  cache[lid]->revoke_count++;
  if(cache[lid]->ls == FREE && cache[lid]->wait_count == 0) {
    int r;
    // printf("release lock lid= %u to server in revoke_handler.\n", lid);
    cache[lid]->ls = RELEASING;
    pthread_mutex_unlock(&m);
    
    std::cout<<"dorelease called in revoke"<<std::endl;  
    // notify the user, extent_client, the comming lid releasing
    lu->dorelease(lid); 

    lock_protocol::status ret = cl->call(lock_protocol::release, lid, id, r);
    pthread_mutex_lock(&m);
    cache[lid]->ls = NONE;
    cache[lid]->is_revoked = false;
    cache[lid]->revoke_count--;
    VERIFY (ret == lock_protocol::OK);
    pthread_cond_broadcast(&cache[lid]->avail_cond);
  }
  // printf("revoke count for lid:%u is %d\n", lid, cache[lid]->revoke_count); 
  pthread_mutex_unlock(&m);
  return ret;
}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid, 
                                 int &)
{
  int ret = rlock_protocol::OK;

  pthread_mutex_lock(&m);
  // printf("retry for lid:%u is called\n", lid);
  cache[lid]->retry = true; 
  pthread_cond_signal(&cache[lid]->retry_cond);
  pthread_mutex_unlock(&m);

  return ret;
}




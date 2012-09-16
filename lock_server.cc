// the lock server implementation
#include <pthread.h>
#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

lock_server::lock_server():
  nacquire (0)
{
  pthread_mutex_init(&mutexLockMap, NULL);
  pthread_cond_init(&lockAvailableCon, NULL);

}

lock_server::~lock_server() 
{
   pthread_mutex_destroy(&mutexLockMap);
   pthread_cond_destroy(&lockAvailableCon);
}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("stat request from clt %d\n", clt);
  r = nacquire;
  return ret;
}

// the reply value r doesn't matter
lock_protocol::status
lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  /* get exclusive access to map */
  pthread_mutex_lock(&mutexLockMap);
  if(lockToStatus.find(lid) == lockToStatus.end()) {
    lockToStatus.insert(std::map<lock_protocol::lockid_t, LockStatus>::value_type(lid, LOCKED));
    // ready to return
  } else {
    // lock exist
    //if(LOCKED == lockToStatus[lid]) {
      // block current thread, and wait for the release
      // of the lock by another thread
      while(LOCKED == lockToStatus[lid]) {
        pthread_cond_wait(&lockAvailableCon, &mutexLockMap);
      }
    //} else {
      lockToStatus[lid] = LOCKED;
   // }
  }
  /* unlock the mutex, let other thread use it */
  pthread_mutex_unlock(&mutexLockMap);

  // the reply vlaue r doesn't matter
  r = 0;

  // since no other place will make ret false, it will always return OK
  // but be aware of the timeout, check that later
  return ret;
}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  /* Enter Critical Section */
  pthread_mutex_lock(&mutexLockMap); 
  // printf("release request from clt %d\n", clt);
 
  // try to release a none-exist lock
  if(lockToStatus.find(lid) == lockToStatus.end()) {
    // logical error : release a non-exist lock
    ret = lock_protocol::RETRY;

  } else if(lockToStatus[lid] == FREE) {
    // logical error : it's already unlocked
    ret = lock_protocol::RETRY;
  } else {
    lockToStatus[lid] = FREE;
    // notify any threads are waiting for this lock
    pthread_cond_broadcast(&lockAvailableCon);  
  }
  // printf("release request from clt %d granted\n", clt);
 
  /* Exit Critical Section */
  pthread_mutex_unlock(&mutexLockMap);

  r = nacquire;

  return ret;
}



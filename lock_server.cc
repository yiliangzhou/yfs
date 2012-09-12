// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

lock_server::lock_server():
  nacquire (0)
{
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
  
  // to do No.1
  if(lockToStatus.find(lid) == lockToStatus.end()) {
    // lockToStatus.insert(lid, LOCKED);
    
  } else {
    // lock exist
    if(LOCKED == lockToStatus[lid]) {
      // block current thread
      // and wait for a signal
    } else {
      lockToStatus[lid] = LOCKED;
      // this will block other acquire requests
    }
  }
  
  // the reply vlaue r doesn't matter
  r = nacquire;

  // since no other place will make ret false, it will always return OK
  // but be aware of the timeout, check that later
  return ret;
}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  // printf("release request from clt %d\n", clt);
  
  // VERIFY(lockToStatus[lid] == LOCKED)
  lockToStatus[lid] = FREE;

  // notify any threads are waiting for the lock

  r = nacquire;
  return ret;
}



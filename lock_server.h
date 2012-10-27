// this is the lock server
// the lock client has a similar interface

#ifndef lock_server_h
#define lock_server_h

#include <pthread.h>
#include <string>
#include "lock_protocol.h"
#include "lock_client.h"
#include "rpc.h"
#include <map>

class lock_server {

 protected:
  int nacquire;

  
  // each lock is identified by an integer of type lock_protocol::lockit_t
  // use std::map to hold the table of lock states
  enum LockStatus { FREE, LOCKED };

  typedef std::map<lock_protocol::lockid_t, LockStatus> LockToStatus;
  LockToStatus lockToStatus;
  // mutex for the global shared lock data for a server
  pthread_mutex_t mutexLockMap;
  pthread_cond_t lockAvailableCon;

 public:
  lock_server();
  ~lock_server();
  lock_protocol::status stat(int clt, lock_protocol::lockid_t lid, int &);
  lock_protocol::status acquire(int clt, lock_protocol::lockid_t lid, int &);
  lock_protocol::status release(int clt, lock_protocol::lockid_t lid, int &);
};

#endif 








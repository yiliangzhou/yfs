// this is the lock server
// the lock client has a similar interface

#ifndef lock_server_h
#define lock_server_h

#include <string>
#include "lock_protocol.h"
#include "lock_client.h"
#include "rpc.h"

class lock_server {

 protected:
  int nacquire;
  
  // each lock is identified by an integer of type lock_protocol::lockit_t
  // use std::map to hold the table of lock states
  enum LockStatus { FREE, LOCKED };

  typedef std::map<lock_protocol::lockid_t, LockStatus> LockToStatus;
  LockToStatus lockToStatus;


 public:
  lock_server();
  ~lock_server() {};
  lock_protocol::status stat(int clt, lock_protocol::lockid_t lid, int &);
  lock_protocol::status acquire(int clt, lock_protocol::lockid_t lid, int &);
  lock_protocol::status release(int clt, lock_protocol::lockid_t lid, int &);
};

#endif 








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
        // lock does not exist, create one and locked it
        lockToStatus.insert(std::map<lock_protocol::lockid_t, LockStatus>::value_type(lid, LOCKED));
    } else {
        // lock exist
        while(LOCKED == lockToStatus[lid]) {
            // as long as the lock is locked, wait in this loop
            pthread_cond_wait(&lockAvailableCon, &mutexLockMap);
        }
        // gain access to the lock, lock it
        lockToStatus[lid] = LOCKED;
    }
    /* unlock the mutex, let other thread use it */
    pthread_mutex_unlock(&mutexLockMap);

    // the reply vlaue r doesn't matter
    // this is the returned vlaue at client side
    r = 0;

    return ret;
}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
    lock_protocol::status ret = lock_protocol::OK;
    // return value at client side
    r = 0;
    /* Enter Critical Section */
    pthread_mutex_lock(&mutexLockMap); 
 
    if(lockToStatus.find(lid) == lockToStatus.end()) {
        // logical error : release a non-exist lock
        ret = lock_protocol::RETRY;
        // r should be assigned properly
        // r = ?
    } else if(lockToStatus[lid] == FREE) {
        // logical error : it's already unlocked
        ret = lock_protocol::RETRY;
        // r = ?
    } else {
        lockToStatus[lid] = FREE;
        // notify threads, if any,  who are waiting for this lock
        pthread_cond_broadcast(&lockAvailableCon);
    }
 
    /* Exit Critical Section */
    pthread_mutex_unlock(&mutexLockMap);

    return ret;
}



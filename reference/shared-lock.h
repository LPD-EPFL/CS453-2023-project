#pragma once

// Requested feature: pthread_rwlock_t
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef __USE_XOPEN2K
#define __USE_XOPEN2K
#endif

#include <pthread.h>
#include <stdbool.h>

/**
 * @brief A lock that can be taken exclusively but also shared. Contrarily to
 * exclusive locks, shared locks do not have wait/wake_up capabilities.
 */
struct shared_lock_t {
    pthread_rwlock_t rwlock;
};

/** Initialize the given lock.
 * @param lock Lock to initialize
 * @return Whether the operation is a success
**/
bool shared_lock_init(struct shared_lock_t* lock);

/** Clean the given lock up.
 * @param lock Lock to clean up
**/
void shared_lock_cleanup(struct shared_lock_t* lock);

/** Wait and acquire the given lock exclusively.
 * @param lock Lock to acquire
 * @return Whether the operation is a success
**/
bool shared_lock_acquire(struct shared_lock_t* lock);

/** Release the given lock that has been taken exclusively.
 * @param lock Lock to release
**/
void shared_lock_release(struct shared_lock_t* lock);

/** Wait and acquire the given lock non-exclusively.
 * @param lock Lock to acquire
 * @return Whether the operation is a success
**/
bool shared_lock_acquire_shared(struct shared_lock_t* lock);

/** Release the given lock that has been taken non-exclusively.
 * @param lock Lock to release
**/
void shared_lock_release_shared(struct shared_lock_t* lock);

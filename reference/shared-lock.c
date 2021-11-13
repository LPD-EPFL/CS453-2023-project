#include "shared-lock.h"

bool shared_lock_init(struct shared_lock_t* lock) {
    return pthread_rwlock_init(&lock->rwlock, NULL) == 0;
}

void shared_lock_cleanup(struct shared_lock_t* lock) {
    pthread_rwlock_destroy(&lock->rwlock);
}

bool shared_lock_acquire(struct shared_lock_t* lock) {
    return pthread_rwlock_wrlock(&lock->rwlock) == 0;
}

void shared_lock_release(struct shared_lock_t* lock) {
    pthread_rwlock_unlock(&lock->rwlock);
}

bool shared_lock_acquire_shared(struct shared_lock_t* lock) {
    return pthread_rwlock_rdlock(&lock->rwlock) == 0;
}

void shared_lock_release_shared(struct shared_lock_t* lock) {
    pthread_rwlock_unlock(&lock->rwlock);
}

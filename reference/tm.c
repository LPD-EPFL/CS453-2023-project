/**
 * @file   tm.c
 * @author Sébastien Rouault <sebastien.rouault@epfl.ch>
 *
 * @section LICENSE
 *
 * Copyright © 2018-2019 Sébastien Rouault.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * any later version. Please see https://gnu.org/licenses/gpl.html
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * @section DESCRIPTION
 *
 * Lock-based transaction manager implementation used as the reference.
**/

// Compile-time configuration
// #define USE_MM_PAUSE
// #define USE_PTHREAD_LOCK
// #define USE_TICKET_LOCK
#define USE_RW_LOCK

// Requested features
#define _GNU_SOURCE
#define _POSIX_C_SOURCE   200809L
#ifdef __STDC_NO_ATOMICS__
    #error Current C11 compiler does not support atomic operations
#endif

// External headers
#include <pthread.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#if (defined(__i386__) || defined(__x86_64__)) && defined(USE_MM_PAUSE)
    #include <xmmintrin.h>
#else
    #include <sched.h>
#endif

// Internal headers
#include <tm.h>

// -------------------------------------------------------------------------- //

/** Define a proposition as likely true.
 * @param prop Proposition
**/
#undef likely
#ifdef __GNUC__
    #define likely(prop) \
        __builtin_expect((prop) ? 1 : 0, 1)
#else
    #define likely(prop) \
        (prop)
#endif

/** Define a proposition as likely false.
 * @param prop Proposition
**/
#undef unlikely
#ifdef __GNUC__
    #define unlikely(prop) \
        __builtin_expect((prop) ? 1 : 0, 0)
#else
    #define unlikely(prop) \
        (prop)
#endif

/** Define one or several attributes.
 * @param type... Attribute names
**/
#undef as
#ifdef __GNUC__
    #define as(type...) \
        __attribute__((type))
#else
    #define as(type...)
    #warning This compiler has no support for GCC attributes
#endif

// -------------------------------------------------------------------------- //

/** Compute a pointer to the parent structure.
 * @param ptr    Member pointer
 * @param type   Parent type
 * @param member Member name
 * @return Parent pointer
**/
#define objectof(ptr, type, member) \
    ((type*) ((uintptr_t) ptr - offsetof(type, member)))

struct link {
    struct link* prev; // Previous link in the chain
    struct link* next; // Next link in the chain
};

/** Link reset.
 * @param link Link to reset
**/
static void link_reset(struct link* link) {
    link->prev = link;
    link->next = link;
}

/** Link insertion before a "base" link.
 * @param link Link to insert
 * @param base Base link relative to which 'link' will be inserted
**/
static void link_insert(struct link* link, struct link* base) {
    struct link* prev = base->prev;
    link->prev = prev;
    link->next = base;
    base->prev = link;
    prev->next = link;
}

/** Link removal.
 * @param link Link to remove
**/
static void link_remove(struct link* link) {
    struct link* prev = link->prev;
    struct link* next = link->next;
    prev->next = next;
    next->prev = prev;
}

// -------------------------------------------------------------------------- //

/** Pause for a very short amount of time.
**/
static inline void pause() {
#if (defined(__i386__) || defined(__x86_64__)) && defined(USE_MM_PAUSE)
    _mm_pause();
#else
    sched_yield();
#endif
}

#if defined(USE_PTHREAD_LOCK)

struct lock_t {
    pthread_mutex_t mutex;
};

/** Initialize the given lock.
 * @param lock Lock to initialize
 * @return Whether the operation is a success
**/
static bool lock_init(struct lock_t* lock) {
    return pthread_mutex_init(&(lock->mutex), NULL) == 0;
}

/** Clean the given lock up.
 * @param lock Lock to clean up
**/
static void lock_cleanup(struct lock_t* lock) {
    pthread_mutex_destroy(&(lock->mutex));
}

/** Wait and acquire the given lock.
 * @param lock Lock to acquire
 * @return Whether the operation is a success
**/
static bool lock_acquire(struct lock_t* lock) {
    return pthread_mutex_lock(&(lock->mutex)) == 0;
}

/** Release the given lock.
 * @param lock Lock to release
**/
static void lock_release(struct lock_t* lock) {
    pthread_mutex_unlock(&(lock->mutex));
}

static bool lock_acquire_shared(struct lock_t* lock) {
    return lock_acquire(lock);
}

static void lock_release_shared(struct lock_t* lock) {
    lock_release(lock);
}

#elif defined(USE_TICKET_LOCK)

struct lock_t {
    atomic_ulong pass; // Ticket that acquires the lock
    atomic_ulong take; // Ticket the next thread takes
};

/** Initialize the given lock.
 * @param lock Lock to initialize
 * @return Whether the operation is a success
**/
static bool lock_init(struct lock_t* lock) {
    atomic_init(&(lock->pass), 0ul);
    atomic_init(&(lock->take), 0ul);
    return true;
}

/** Clean the given lock up.
 * @param lock Lock to clean up
**/
static void lock_cleanup(struct lock_t* lock as(unused)) {
    return;
}

/** Wait and acquire the given lock.
 * @param lock Lock to acquire
 * @return Whether the operation is a success
**/
static bool lock_acquire(struct lock_t* lock) {
    unsigned long ticket = atomic_fetch_add_explicit(&(lock->take), 1ul, memory_order_relaxed);
    while (atomic_load_explicit(&(lock->pass), memory_order_relaxed) != ticket)
        pause();
    atomic_thread_fence(memory_order_acquire);
    return true;
}

/** Release the given lock.
 * @param lock Lock to release
**/
static void lock_release(struct lock_t* lock) {
    atomic_fetch_add_explicit(&(lock->pass), 1, memory_order_release);
}

static bool lock_acquire_shared(struct lock_t* lock) {
    return lock_acquire(lock);
}

static void lock_release_shared(struct lock_t* lock) {
    lock_release(lock);
}

#elif defined(USE_RW_LOCK)

struct lock_t {
    pthread_rwlock_t rwlock;
};

/** Initialize the given lock.
 * @param lock Lock to initialize
 * @return Whether the operation is a success
**/
static bool lock_init(struct lock_t* lock) {
    return (0 == pthread_rwlock_init(&lock->rwlock, NULL));
}

/** Clean the given lock up.
 * @param lock Lock to clean up
**/
static void lock_cleanup(struct lock_t* lock as(unused)) {
    pthread_rwlock_destroy(&lock->rwlock);
}

/** Wait and acquire the given lock.
 * @param lock Lock to acquire
 * @return Whether the operation is a success
**/
static bool lock_acquire(struct lock_t* lock) {
    return (0 == pthread_rwlock_wrlock(&lock->rwlock));
}

/** Release the given lock.
 * @param lock Lock to release
**/
static void lock_release(struct lock_t* lock) {
    pthread_rwlock_unlock(&lock->rwlock);
}

/** Wait and acquire the given lock.
 * @param lock Lock to acquire
 * @return Whether the operation is a success
**/
static bool lock_acquire_shared(struct lock_t* lock) {
    return (0 == pthread_rwlock_rdlock(&lock->rwlock));
}

/** Release the given lock.
 * @param lock Lock to release
**/
static void lock_release_shared(struct lock_t* lock) {
    pthread_rwlock_unlock(&lock->rwlock);
}

#else // Test-and-test-and-set

struct lock_t {
    atomic_bool locked; // Whether the lock is taken
};

/** Initialize the given lock.
 * @param lock Lock to initialize
 * @return Whether the operation is a success
**/
static bool lock_init(struct lock_t* lock) {
    atomic_init(&(lock->locked), false);
    return true;
}

/** Clean the given lock up.
 * @param lock Lock to clean up
**/
static void lock_cleanup(struct lock_t* lock as(unused)) {
    return;
}

/** Wait and acquire the given lock.
 * @param lock Lock to acquire
 * @return Whether the operation is a success
**/
static bool lock_acquire(struct lock_t* lock) {
    bool expected = false;
    while (unlikely(!atomic_compare_exchange_weak_explicit(&(lock->locked), &expected, true, memory_order_acquire, memory_order_relaxed))) {
        expected = false;
        while (unlikely(atomic_load_explicit(&(lock->locked), memory_order_relaxed)))
            pause();
    }
    return true;
}

/** Release the given lock.
 * @param lock Lock to release
**/
static void lock_release(struct lock_t* lock) {
    atomic_store_explicit(&(lock->locked), false, memory_order_release);
}

static bool lock_acquire_shared(struct lock_t* lock) {
    return lock_acquire(lock);
}

static void lock_release_shared(struct lock_t* lock) {
    lock_release(lock);
}

#endif

// -------------------------------------------------------------------------- //

static const tx_t read_only_tx  = UINTPTR_MAX - 10;
static const tx_t read_write_tx = UINTPTR_MAX - 11;

struct region {
    struct lock_t lock; // Global lock
    void* start;        // Start of the shared memory region
    struct link allocs; // Allocated shared memory regions
    size_t size;        // Size of the shared memory region (in bytes)
    size_t align;       // Claimed alignment of the shared memory region (in bytes)
    size_t align_alloc; // Actual alignment of the memory allocations (in bytes)
    size_t delta_alloc; // Space to add at the beginning of the segment for the link chain (in bytes)
};

shared_t tm_create(size_t size, size_t align) {
    struct region* region = (struct region*) malloc(sizeof(struct region));
    if (unlikely(!region)) {
        return invalid_shared;
    }
    size_t align_alloc = align < sizeof(void*) ? sizeof(void*) : align; // Also satisfy alignment requirement of 'struct link'
    if (unlikely(posix_memalign(&(region->start), align_alloc, size) != 0)) {
        free(region);
        return invalid_shared;
    }
    if (unlikely(!lock_init(&(region->lock)))) {
        free(region->start);
        free(region);
        return invalid_shared;
    }
    memset(region->start, 0, size);
    link_reset(&(region->allocs));
    region->size        = size;
    region->align       = align;
    region->align_alloc = align_alloc;
    region->delta_alloc = (sizeof(struct link) + align_alloc - 1) / align_alloc * align_alloc;
    return region;
}

void tm_destroy(shared_t shared) {
    struct region* region = (struct region*) shared;
    struct link* allocs = &(region->allocs);
    while (true) { // Free allocated segments
        struct link* alloc = allocs->next;
        if (alloc == allocs)
            break;
        link_remove(alloc);
        free(alloc);
    }
    free(region->start);
    free(region);
    lock_cleanup(&(region->lock));
}

void* tm_start(shared_t shared) {
    return ((struct region*) shared)->start;
}

size_t tm_size(shared_t shared) {
    return ((struct region*) shared)->size;
}

size_t tm_align(shared_t shared) {
    return ((struct region*) shared)->align;
}

tx_t tm_begin(shared_t shared, bool is_ro) {
    if (is_ro) {
        if (unlikely(!lock_acquire_shared(&(((struct region*) shared)->lock))))
            return invalid_tx;
        return read_only_tx;
    } else {
        if (unlikely(!lock_acquire(&(((struct region*) shared)->lock))))
            return invalid_tx;
        return read_write_tx;
    }
}

bool tm_end(shared_t shared, tx_t tx) {
    if (tx == read_only_tx) {
        lock_release_shared(&(((struct region*) shared)->lock));
    } else {
        lock_release(&(((struct region*) shared)->lock));
    }
    return true;
}

bool tm_read(shared_t shared as(unused), tx_t tx as(unused), void const* source, size_t size, void* target) {
    memcpy(target, source, size);
    return true;
}

bool tm_write(shared_t shared as(unused), tx_t tx as(unused), void const* source, size_t size, void* target) {
    memcpy(target, source, size);
    return true;
}

alloc_t tm_alloc(shared_t shared, tx_t tx as(unused), size_t size, void** target) {
    size_t align_alloc = ((struct region*) shared)->align_alloc;
    size_t delta_alloc = ((struct region*) shared)->delta_alloc;
    void* segment;
    if (unlikely(posix_memalign(&segment, align_alloc, delta_alloc + size) != 0)) // Allocation failed
        return nomem_alloc;
    link_insert((struct link*) segment, &(((struct region*) shared)->allocs));
    segment = (void*) ((uintptr_t) segment + delta_alloc);
    memset(segment, 0, size);
    *target = segment;
    return success_alloc;
}

bool tm_free(shared_t shared, tx_t tx as(unused), void* segment) {
    size_t delta_alloc = ((struct region*) shared)->delta_alloc;
    segment = (void*) ((uintptr_t) segment - delta_alloc);
    link_remove((struct link*) segment);
    free(segment);
    return true;
}

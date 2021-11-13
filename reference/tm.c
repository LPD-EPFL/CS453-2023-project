/**
 * @file   tm.c
 * @author Sébastien Rouault <sebastien.rouault@epfl.ch>
 * @author Antoine Murat <antoine.murat@epfl.ch>
 *
 * @section LICENSE
 *
 * Copyright © 2018-2021 Sébastien Rouault.
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

// Requested feature: posix_memalign
#define _POSIX_C_SOURCE   200809L

// External headers
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

// Internal headers
#include <tm.h>

#include "macros.h"
#include "shared-lock.h"

static const tx_t read_only_tx  = UINTPTR_MAX - 10;
static const tx_t read_write_tx = UINTPTR_MAX - 11;

/**
 * @brief List of dynamically allocated segments.
 */
struct segment_node {
    struct segment_node* prev;
    struct segment_node* next;
    // uint8_t segment[] // segment of dynamic size
};
typedef struct segment_node* segment_list;

/**
 * @brief Simple Shared Memory Region (a.k.a Transactional Memory).
 */
struct region {
    struct shared_lock_t lock; // Global (coarse-grained) lock
    void* start;        // Start of the shared memory region (i.e., of the non-deallocable memory segment)
    segment_list allocs; // Shared memory segments dynamically allocated via tm_alloc within transactions
    size_t size;        // Size of the non-deallocable memory segment (in bytes)
    size_t align;       // Size of a word in the shared memory region (in bytes)
};

shared_t tm_create(size_t size, size_t align) {
    struct region* region = (struct region*) malloc(sizeof(struct region));
    if (unlikely(!region)) {
        return invalid_shared;
    }
    // We allocate the shared memory buffer such that its words are correctly
    // aligned.
    if (posix_memalign(&(region->start), align, size) != 0) {
        free(region);
        return invalid_shared;
    }
    if (!shared_lock_init(&(region->lock))) {
        free(region->start);
        free(region);
        return invalid_shared;
    }
    memset(region->start, 0, size);
    region->allocs      = NULL;
    region->size        = size;
    region->align       = align;
    return region;
}

void tm_destroy(shared_t shared) {
    // Note: To be compatible with any implementation, shared_t is defined as a
    // void*. For this particular implementation, the "real" type of a shared_t
    // is a struct region*.
    struct region* region = (struct region*) shared;
    while (region->allocs) { // Free allocated segments
        segment_list tail = region->allocs->next;
        free(region->allocs);
        region->allocs = tail;
    }
    free(region->start);
    shared_lock_cleanup(&(region->lock));
    free(region);
}

// Note: In this particular implementation, tm_start returns a valid virtual
// address (i.e., shared memory locations are virtually addressed).
// This is NOT required. Indeed, as the content of shared memory is only ever
// accessed via tm functions (read/write/free), you can use any naming scheme
// you want to designate a word within the transactional memory as long as it
// fits in a void*. Said functions will need to translate from a void* to a
// specific word. Moreover, your naming scheme should support pointer arithmetic
// (i.e., one should be able to pass tm_start(shared)+align*n to access the
// (n+1)-th word within a memory region).
// You can assume sizeof(void*) == 64b and that the maximum size ever allocated
// will be 2^48.
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
    // We let read-only transactions run in parallel by acquiring a shared
    // access. On the other hand, read-write transactions acquire an exclusive
    // access. At any point in time, the lock can be shared between any number
    // of read-only transactions or held by a single read-write transaction.
    if (is_ro) {
        // Note: "unlikely" is a macro that helps branch prediction.
        // It tells the compiler (GCC) that the condition is unlikely to be true
        // and to optimize the code with this additional knowledge.
        // It of course penalizes executions in which the condition turns up to
        // be true.
        if (unlikely(!shared_lock_acquire_shared(&(((struct region*) shared)->lock))))
            return invalid_tx;
        return read_only_tx;
    } else {
        if (unlikely(!shared_lock_acquire(&(((struct region*) shared)->lock))))
            return invalid_tx;
        return read_write_tx;
    }
}

bool tm_end(shared_t shared, tx_t tx) {
    if (tx == read_only_tx) {
        shared_lock_release_shared(&(((struct region*) shared)->lock));
    } else {
        shared_lock_release(&(((struct region*) shared)->lock));
    }
    return true;
}

// Note: "unused" is a macro that tells the compiler that a variable is unused.
bool tm_read(shared_t unused(shared), tx_t unused(tx), void const* source, size_t size, void* target) {
    memcpy(target, source, size);
    return true;
}

bool tm_write(shared_t unused(shared), tx_t unused(tx), void const* source, size_t size, void* target) {
    memcpy(target, source, size);
    return true;
}

alloc_t tm_alloc(shared_t shared, tx_t unused(tx), size_t size, void** target) {
    // We allocate the dynamic segment such that its words are correctly
    // aligned. Moreover, the alignment of the 'next' and 'prev' pointers must
    // be satisfied. Thus, we use align on max(align, struct segment_node*).
    size_t align = ((struct region*) shared)->align;
    align = align < sizeof(struct segment_node*) ? sizeof(void*) : align;

    struct segment_node* sn;
    if (unlikely(posix_memalign((void**)&sn, align, sizeof(struct segment_node) + size) != 0)) // Allocation failed
        return nomem_alloc;

    // Insert in the linked list
    sn->prev = NULL;
    sn->next = ((struct region*) shared)->allocs;
    if (sn->next) sn->next->prev = sn;
    ((struct region*) shared)->allocs = sn;

    void* segment = (void*) ((uintptr_t) sn + sizeof(struct segment_node));
    memset(segment, 0, size);
    *target = segment;
    return success_alloc;
}

bool tm_free(shared_t shared, tx_t unused(tx), void* segment) {
    struct segment_node* sn = (struct segment_node*) ((uintptr_t) segment - sizeof(struct segment_node));

    // Remove from the linked list
    if (sn->prev) sn->prev->next = sn->next;
    else ((struct region*) shared)->allocs = sn->next;
    if (sn->next) sn->next->prev = sn->prev;

    free(sn);
    return true;
}

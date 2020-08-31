/**
 * @file   entrypoint.cpp
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
 * "Entry point" source file, implementing the playground function 'entry_point' and the lock.
**/

// External headers
#include <atomic>
#include <iostream>
#include <mutex>

// Internal headers
#include "entrypoint.hpp"
#include "runner.hpp"

// -------------------------------------------------------------------------- //
// Lock implementation, that you have to complete

// NOTE: You may want to add data member(s) in 'class Lock' at entrypoint.hpp:30

/** Lock default constructor.
**/
Lock::Lock() {
    // ...
}

/** Lock destructor.
**/
Lock::~Lock() {
    // ...
}

/** [thread-safe] Acquire the lock, block if it is already acquired.
**/
void Lock::lock() {
    // ...
}

/** [thread-safe] Release the lock, assuming it is indeed held by the caller.
**/
void Lock::unlock() {
    // ...
}

// -------------------------------------------------------------------------- //
// Thread accessing the shared memory (a mere shared counter in this program)

/** Thread entry point.
 * @param nb   Total number of threads
 * @param id   This thread ID (from 0 to nb-1 included)
 * @param lock Lock to use to protect the shared memory (read & written by 'shared_access')
**/
void entry_point(size_t nb, size_t id, Lock& lock) {
    ::printf("Hello from thread %lu/%lu\n", id, nb);
    for (int i = 0; i < 10000; ++i) {
        ::std::lock_guard<Lock> guard{lock}; // Lock is acquired here
        ::shared_access();
        // Lock is automatically released here (thanks to 'lock_guard', upon leaving the scope)
    }
}

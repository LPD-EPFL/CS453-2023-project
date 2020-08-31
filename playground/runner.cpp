/**
 * @file   runner.cpp
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
 * Trivial program that call a function in several threads.
**/

// External headers
#include <atomic>
#include <iostream>
#include <thread>

// Internal headers
#include "entrypoint.hpp"
#include "runner.hpp"

// -------------------------------------------------------------------------- //
// Shared memory, access function and consistency check

static int counter = 0;
static ::std::atomic<int> check_counter{0};

/** Performs some operations on some shared memory.
**/
void shared_access() {
    ++counter;
    check_counter.fetch_add(1, ::std::memory_order_relaxed);
}

/** (Empirically) checks that concurrent operations did not break consistency, warn accordingly.
**/
static void shared_check() {
    auto calls = check_counter.load(::std::memory_order_relaxed);
    if (counter == calls) {
        ::std::cout << "** No inconsistency detected (" << counter << " == " << calls << ") **" << ::std::endl;
    } else {
        ::std::cout << "** Inconsistency detected (" << counter << " != " << calls << ") **" << ::std::endl;
    }
}

// -------------------------------------------------------------------------- //
// Lock + thread launches and management

/** Program entry point.
 * @param argc Arguments count
 * @param argv Arguments values
 * @return Program return code
**/
int main(int, char**) {
    auto const nbworkers = []() {
        auto res = ::std::thread::hardware_concurrency();
        if (res == 0) {
            ::std::cout << "WARNING: unable to query '::std::thread::hardware_concurrency()', falling back to 4 threads" << ::std::endl;
            res = 4;
        }
        return static_cast<size_t>(res);
    }();
    Lock lock;
    ::std::thread threads[nbworkers];
    for (size_t i = 0; i < nbworkers; ++i) {
        threads[i] = ::std::thread{[&](size_t i) {
            entry_point(nbworkers, i, lock);
        }, i};
    }
    for (auto&& thread: threads)
        thread.join();
    shared_check();
    return 0;
}

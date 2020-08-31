/**
 * @file   entrypoint.hpp
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
 * Interface for the "entry point" source file.
**/

#pragma once

// -------------------------------------------------------------------------- //

/** Your lock class.
**/
class Lock final {
public:
    /** Deleted copy/move constructor/assignment.
    **/
    Lock(Lock const&) = delete;
    Lock& operator=(Lock const&) = delete;
    // NOTE: Actually, one could argue it makes sense to implement move,
    //       but we don't care about this feature in our simple playground
public:
    Lock();
    ~Lock();
public:
    void lock();
    void unlock();
};

// -------------------------------------------------------------------------- //

void entry_point(size_t, size_t, Lock&);

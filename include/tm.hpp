/**
 * @file   tm.hpp
 * @author Sébastien ROUAULT <sebastien.rouault@epfl.ch>
 *
 * @section LICENSE
 *
 * Copyright © 2018-2019 Sébastien ROUAULT.
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
 * Interface declaration for the transaction manager to use (C++ version).
 * YOU SHOULD NOT MODIFY THIS FILE.
**/

#pragma once

#include <cstddef>
#include <cstdint>

// -------------------------------------------------------------------------- //

using shared_t = void*;
constexpr static shared_t invalid_shared = nullptr; // Invalid shared memory region

using tx_t = uintptr_t;
constexpr static tx_t invalid_tx = ~(tx_t(0)); // Invalid transaction constant

enum class Alloc: int {
    success = 0, // Allocation successful and the TX can continue
    abort   = 1, // TX was aborted and could be retried
    nomem   = 2  // Memory allocation failed but TX was not aborted
};

// -------------------------------------------------------------------------- //

extern "C" {
    shared_t tm_create(size_t, size_t) noexcept;
    void     tm_destroy(shared_t) noexcept;
    void*    tm_start(shared_t) noexcept;
    size_t   tm_size(shared_t) noexcept;
    size_t   tm_align(shared_t) noexcept;
    tx_t     tm_begin(shared_t, bool) noexcept;
    bool     tm_end(shared_t, tx_t) noexcept;
    bool     tm_read(shared_t, tx_t, void const*, size_t, void*) noexcept;
    bool     tm_write(shared_t, tx_t, void const*, size_t, void*) noexcept;
    Alloc    tm_alloc(shared_t, tx_t, size_t, void**) noexcept;
    bool     tm_free(shared_t, tx_t, void*) noexcept;
}

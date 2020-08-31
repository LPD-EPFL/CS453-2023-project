/**
 * @file   transactional.hpp
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
 * Transactional memory library management and use.
**/

#pragma once

// External headers
extern "C" {
#include <dlfcn.h>
#include <limits.h>
}

// Internal headers
namespace STM {
#include <tm.hpp>
}
#include "common.hpp"

// -------------------------------------------------------------------------- //
namespace Exception {

/** Exception tree.
**/
EXCEPTION(Path, Any, "path exception");
    EXCEPTION(PathResolve, Path, "unable to resolve the given path");
EXCEPTION(Module, Any, "transaction library exception");
    EXCEPTION(ModuleLoading, Module, "unable to load a transaction library");
    EXCEPTION(ModuleSymbol, Module, "symbol not found in loaded libraries");
EXCEPTION(Transaction, Any, "transaction manager exception");
    EXCEPTION(TransactionAlign, Transaction, "incorrect alignment detected before transactional operation");
    EXCEPTION(TransactionReadOnly, Transaction, "tried to write/alloc/free using a read-only transaction");
    EXCEPTION(TransactionCreate, Transaction, "shared memory region creation failed");
    EXCEPTION(TransactionBegin, Transaction, "transaction begin failed");
    EXCEPTION(TransactionAlloc, Transaction, "memory allocation failed (insufficient memory)");
    EXCEPTION(TransactionRetry, Transaction, "transaction aborted and can be retried");
    EXCEPTION(TransactionNotLastSegment, Transaction, "trying to deallocate the first segment");
EXCEPTION(Shared, Any, "operation in shared memory exception");
    EXCEPTION(SharedAlign, Shared, "address in shared memory is not properly aligned for the specified type");
    EXCEPTION(SharedOverflow, Shared, "index is past array length");
    EXCEPTION(SharedDoubleAlloc, Shared, "(probable) double allocation detected before transactional operation");
    EXCEPTION(SharedDoubleFree, Shared, "double free detected before transactional operation");

}
// -------------------------------------------------------------------------- //

/** Transactional library management class.
**/
class TransactionalLibrary final: private NonCopyable {
    friend class TransactionalMemory;
private:
    /** Function types.
    **/
    using FnCreate  = decltype(&STM::tm_create);
    using FnDestroy = decltype(&STM::tm_destroy);
    using FnStart   = decltype(&STM::tm_start);
    using FnSize    = decltype(&STM::tm_size);
    using FnAlign   = decltype(&STM::tm_align);
    using FnBegin   = decltype(&STM::tm_begin);
    using FnEnd     = decltype(&STM::tm_end);
    using FnRead    = decltype(&STM::tm_read);
    using FnWrite   = decltype(&STM::tm_write);
    using FnAlloc   = decltype(&STM::tm_alloc);
    using FnFree    = decltype(&STM::tm_free);
private:
    void*     module;     // Module opaque handler
    FnCreate  tm_create;  // Module's initialization function
    FnDestroy tm_destroy; // Module's cleanup function
    FnStart   tm_start;   // Module's start address query function
    FnSize    tm_size;    // Module's size query function
    FnAlign   tm_align;   // Module's alignment query function
    FnBegin   tm_begin;   // Module's transaction begin function
    FnEnd     tm_end;     // Module's transaction end function
    FnRead    tm_read;    // Module's shared memory read function
    FnWrite   tm_write;   // Module's shared memory write function
    FnAlloc   tm_alloc;   // Module's shared memory allocation function
    FnFree    tm_free;    // Module's shared memory freeing function
private:
    /** Solve a symbol from its name, and bind it to the given function.
     * @param name Name of the symbol to resolve
     * @param func Target function to bind (optional, to use template parameter deduction)
    **/
    template<class Signature> auto solve(char const* name) const {
        auto res = ::dlsym(module, name);
        if (unlikely(!res))
            throw Exception::ModuleSymbol{};
        return *reinterpret_cast<Signature*>(&res);
    }
    template<class Signature> void solve(char const* name, Signature& func) const {
        func = solve<Signature>(name);
    }
public:
    /** Loader constructor.
     * @param path  Path to the library to load
    **/
    TransactionalLibrary(char const* path) {
        { // Resolve path and load module
            char resolved[PATH_MAX];
            if (unlikely(!realpath(path, resolved)))
                throw Exception::PathResolve{};
            module = ::dlopen(resolved, RTLD_NOW | RTLD_LOCAL);
            if (unlikely(!module))
                throw Exception::ModuleLoading{};
        }
        { // Bind module's 'tm_*' symbols
            solve("tm_create", tm_create);
            solve("tm_destroy", tm_destroy);
            solve("tm_start", tm_start);
            solve("tm_size", tm_size);
            solve("tm_align", tm_align);
            solve("tm_begin", tm_begin);
            solve("tm_end", tm_end);
            solve("tm_read", tm_read);
            solve("tm_write", tm_write);
            solve("tm_alloc", tm_alloc);
            solve("tm_free", tm_free);
        }
    }
    /** Unloader destructor.
    **/
    ~TransactionalLibrary() noexcept {
        ::dlclose(module); // Close loaded module
    }
};

/** One shared memory region management class.
**/
class TransactionalMemory final: private NonCopyable {
private:
    /** Check whether the given alignment is a power of 2
    **/
    constexpr static bool is_power_of_two(size_t align) noexcept {
        return align != 0 && (align & (align - 1)) == 0;
    }
public:
    /** Opaque shared memory region handle class.
    **/
    using Shared = STM::shared_t;
    /** Transaction class alias.
    **/
    using TX = STM::tx_t;
private:
    TransactionalLibrary const& tl; // Bound transactional library
    Shared shared;     // Handle of the shared memory region used
    void*  start_addr; // Shared memory region first segment's start address
    size_t start_size; // Shared memory region first segment's size (in bytes)
    size_t alignment;  // Shared memory region alignment (in bytes)
public:
    /** Bind constructor.
     * @param library Transactional library to use
     * @param align   Shared memory region required alignment
     * @param size    Size of the shared memory region to allocate
    **/
    TransactionalMemory(TransactionalLibrary const& library, size_t align, size_t size): tl{library}, start_size{size}, alignment{align} {
        if (unlikely(assert_mode && (!is_power_of_two(align) || size % align != 0)))
            throw Exception::TransactionAlign{};
        bounded_run(max_side_time, [&]() {
            shared = tl.tm_create(size, align);
            if (unlikely(shared == STM::invalid_shared))
                throw Exception::TransactionCreate{};
            start_addr = tl.tm_start(shared);
        }, "The transactional library takes too long creating the shared memory");
    }
    /** Unbind destructor.
    **/
    ~TransactionalMemory() noexcept {
        bounded_run(max_side_time, [&]() {
            tl.tm_destroy(shared);
        }, "The transactional library takes too long destroying the shared memory");
    }
public:
    /** [thread-safe] Return the start address of the first shared segment.
     * @return Address of the first allocated shared region
    **/
    auto get_start() const noexcept {
        return start_addr;
    }
    /** [thread-safe] Return the size of the first shared segment.
     * @return Size in the first allocated shared region (in bytes)
    **/
    auto get_size() const noexcept {
        return start_size;
    }
    /** [thread-safe] Get the shared memory region global alignment.
     * @return Global alignment (in bytes)
    **/
    auto get_align() const noexcept {
        return alignment;
    }
public:
    /** [thread-safe] Begin a new transaction on the shared memory region.
     * @param ro Whether the transaction is read-only
     * @return Opaque transaction ID, 'STM::invalid_tx' on failure
    **/
    auto begin(bool ro) const noexcept {
        return tl.tm_begin(shared, ro);
    }
    /** [thread-safe] End the given transaction.
     * @param tx Opaque transaction ID
     * @return Whether the whole transaction is a success
    **/
    auto end(TX tx) const noexcept {
        return tl.tm_end(shared, tx);
    }
    /** [thread-safe] Read operation in the given transaction, source in the shared region and target in a private region.
     * @param tx     Transaction to use
     * @param source Source start address
     * @param size   Source/target range
     * @param target Target start address
     * @return Whether the whole transaction can continue
    **/
    auto read(TX tx, void const* source, size_t size, void* target) const noexcept {
        return tl.tm_read(shared, tx, source, size, target);
    }
    /** [thread-safe] Write operation in the given transaction, source in a private region and target in the shared region.
     * @param tx     Transaction to use
     * @param source Source start address
     * @param size   Source/target range
     * @param target Target start address
     * @return Whether the whole transaction can continue
    **/
    auto write(TX tx, void const* source, size_t size, void* target) const noexcept {
        return tl.tm_write(shared, tx, source, size, target);
    }
    /** [thread-safe] Memory allocation operation in the given transaction, throw if no memory available.
     * @param tx     Transaction to use
     * @param size   Size to allocate
     * @param target Target start address
     * @return Allocation status
    **/
    auto alloc(TX tx, size_t size, void** target) const noexcept {
        return tl.tm_alloc(shared, tx, size, target);
    }
    /** [thread-safe] Memory freeing operation in the given transaction.
     * @param tx     Transaction to use
     * @param target Target start address
     * @return Whether the whole transaction can continue
    **/
    auto free(TX tx, void* target) const noexcept {
        return tl.tm_free(shared, tx, target);
    }
};

/** One transaction over a shared memory region management class.
**/
class Transaction final: private NonCopyable {
public:
    /** Transaction mode class.
    **/
    enum class Mode: bool {
        read_write = false,
        read_only  = true
    };
private:
    TransactionalMemory const& tm; // Bound transactional memory
    STM::tx_t tx; // Opaque transaction handle
    bool aborted; // Transaction was aborted
    bool is_ro;   // Whether the transaction is read-only (solely for assertion)
public:
    /** Deleted copy constructor/assignment.
    **/
    Transaction(Transaction const&) = delete;
    Transaction& operator=(Transaction const&) = delete;
    /** Begin constructor.
     * @param tm Transactional memory to bind
     * @param ro Whether the transaction is read-only
    **/
    Transaction(TransactionalMemory const& tm, Mode ro): tm{tm}, tx{tm.begin(static_cast<bool>(ro))}, aborted{false}, is_ro{static_cast<bool>(ro)} {
        if (unlikely(tx == STM::invalid_tx))
            throw Exception::TransactionBegin{};
    }
    /** End destructor.
    **/
    ~Transaction() noexcept(false) {
        if (likely(!aborted)) {
            if (unlikely(!tm.end(tx)))
                throw Exception::TransactionRetry{};
        }
    }
public:
    /** [thread-safe] Return the bound transactional memory instance.
     * @return Bound transactional memory instance
    **/
    auto const& get_tm() const noexcept {
        return tm;
    }
public:
    /** [thread-safe] Read operation in the bound transaction, source in the shared region and target in a private region.
     * @param source Source start address
     * @param size   Source/target range
     * @param target Target start address
    **/
    void read(void const* source, size_t size, void* target) {
        if (unlikely(!tm.read(tx, source, size, target))) {
            aborted = true;
            throw Exception::TransactionRetry{};
        }
    }
    /** [thread-safe] Write operation in the bound transaction, source in a private region and target in the shared region.
     * @param source Source start address
     * @param size   Source/target range
     * @param target Target start address
    **/
    void write(void const* source, size_t size, void* target) {
        if (unlikely(assert_mode && is_ro))
            throw Exception::TransactionReadOnly{};
        if (unlikely(!tm.write(tx, source, size, target))) {
            aborted = true;
            throw Exception::TransactionRetry{};
        }
    }
    /** [thread-safe] Memory allocation operation in the bound transaction, throw if no memory available.
     * @param size Size to allocate
     * @return Target start address
    **/
    void* alloc(size_t size) {
        if (unlikely(assert_mode && is_ro))
            throw Exception::TransactionReadOnly{};
        void* target;
        switch (tm.alloc(tx, size, &target)) {
        case STM::Alloc::success:
            return target;
        case STM::Alloc::nomem:
            throw Exception::TransactionAlloc{};
        default: // STM::Alloc::abort
            aborted = true;
            throw Exception::TransactionRetry{};
        }
    }
    /** [thread-safe] Memory freeing operation in the bound transaction.
     * @param target Target start address
    **/
    void free(void* target) {
        if (unlikely(assert_mode && is_ro))
            throw Exception::TransactionReadOnly{};
        if (unlikely(!tm.free(tx, target))) {
            aborted = true;
            throw Exception::TransactionRetry{};
        }
    }
};

// -------------------------------------------------------------------------- //

/** Shared read/write helper class.
 * @param Type Specified type (array)
**/
template<class Type> class Shared {
protected:
    Transaction& tx; // Bound transaction
    Type* address; // Address in shared memory
public:
    /** Binding constructor.
     * @param tx      Bound transaction
     * @param address Address to bind to
    **/
    Shared(Transaction& tx, void* address): tx{tx}, address{reinterpret_cast<Type*>(address)} {
        if (unlikely(assert_mode && reinterpret_cast<uintptr_t>(address) % tx.get_tm().get_align() != 0))
            throw Exception::SharedAlign{};
        if (unlikely(assert_mode && reinterpret_cast<uintptr_t>(address) % alignof(Type) != 0))
            throw Exception::SharedAlign{};
    }
public:
    /** Get the address in shared memory.
     * @return Address in shared memory
    **/
    auto get() const noexcept {
        return address;
    }
public:
    /** Read operation.
     * @return Private copy of the content at the shared address
    **/
    Type read() const {
        Type res;
        tx.read(address, sizeof(Type), &res);
        return res;
    }
    operator Type() const {
        return read();
    }
    /** Write operation.
     * @param source Private content to write at the shared address
    **/
    void write(Type const& source) const {
        tx.write(&source, sizeof(Type), address);
    }
    void operator=(Type const& source) const {
        return write(source);
    }
public:
    /** Address of the first byte after the entry.
     * @return First byte after the entry
    **/
    void* after() const noexcept {
        return address + 1;
    }
};
template<class Type> class Shared<Type*> {
protected:
    Transaction& tx; // Bound transaction
    Type** address; // Address in shared memory
public:
    /** Binding constructor.
     * @param tx      Bound transaction
     * @param address Address to bind to
    **/
    Shared(Transaction& tx, void* address): tx{tx}, address{reinterpret_cast<Type**>(address)} {
        if (unlikely(assert_mode && reinterpret_cast<uintptr_t>(address) % tx.get_tm().get_align() != 0))
            throw Exception::SharedAlign{};
        if (unlikely(assert_mode && reinterpret_cast<uintptr_t>(address) % alignof(Type*) != 0))
            throw Exception::SharedAlign{};
    }
public:
    /** Get the address in shared memory.
     * @return Address in shared memory
    **/
    auto get() const noexcept {
        return address;
    }
public:
    /** Read operation.
     * @return Private copy of the content at the shared address
    **/
    Type* read() const {
        Type* res;
        tx.read(address, sizeof(Type*), &res);
        return res;
    }
    operator Type*() const {
        return read();
    }
    /** Write operation.
     * @param source Private content to write at the shared address
    **/
    void write(Type* source) const {
        tx.write(&source, sizeof(Type*), address);
    }
    void operator=(Type* source) const {
        return write(source);
    }
    /** Allocate and write operation.
     * @param size Size to allocate (defaults to size of the underlying class)
     * @return Private copy of the just-written content at the shared address
    **/
    Type* alloc(size_t size = 0) const {
        if (unlikely(assert_mode && read() != nullptr))
            throw Exception::SharedDoubleAlloc{};
        auto addr = tx.alloc(size > 0 ? size: sizeof(Type));
        write(reinterpret_cast<Type*>(addr));
        return reinterpret_cast<Type*>(addr);
    }
    /** Free and write operation.
    **/
    void free() const {
        if (unlikely(assert_mode && read() == nullptr))
            throw Exception::SharedDoubleFree{};
        tx.free(read());
        write(nullptr);
    }
public:
    /** Address of the first byte after the entry.
     * @return First byte after the entry
    **/
    void* after() const noexcept {
        return address + 1;
    }
};
template<class Type> class Shared<Type[]> {
protected:
    Transaction& tx; // Bound transaction
    Type* address; // Address of the first element in shared memory
public:
    /** Binding constructor.
     * @param tx      Bound transaction
     * @param address Address to bind to
    **/
    Shared(Transaction& tx, void* address): tx{tx}, address{reinterpret_cast<Type*>(address)} {
        if (unlikely(assert_mode && reinterpret_cast<uintptr_t>(address) % tx.get_tm().get_align() != 0))
            throw Exception::SharedAlign{};
        if (unlikely(assert_mode && reinterpret_cast<uintptr_t>(address) % alignof(Type) != 0))
            throw Exception::SharedAlign{};
    }
public:
    /** Get the address in shared memory.
     * @return Address in shared memory
    **/
    auto get() const noexcept {
        return address;
    }
public:
    /** Read operation.
     * @param index Index to read
     * @return Private copy of the content at the shared address
    **/
    Type read(size_t index) const {
        Type res;
        tx.read(address + index, sizeof(Type), &res);
        return res;
    }
    /** Write operation.
     * @param index  Index to write
     * @param source Private content to write at the shared address
    **/
    void write(size_t index, Type const& source) const {
        tx.write(tx, &source, sizeof(Type), address + index);
    }
public:
    /** Reference a cell.
     * @param index Cell to reference
     * @return Shared on that cell
    **/
    Shared<Type> operator[](size_t index) const {
        return Shared<Type>{tx, address + index};
    }
    /** Address of the first byte after the entry.
     * @param length Length of the array
     * @return First byte after the entry
    **/
    void* after(size_t length) const noexcept {
        return address + length;
    }
};
template<class Type, size_t n> class Shared<Type[n]> {
protected:
    Transaction& tx; // Bound transaction
    Type* address; // Address of the first element in shared memory
public:
    /** Binding constructor.
     * @param tx      Bound transaction
     * @param address Address to bind to
    **/
    Shared(Transaction& tx, void* address): tx{tx}, address{reinterpret_cast<Type*>(address)} {
        if (unlikely(assert_mode && reinterpret_cast<uintptr_t>(address) % tx.get_tm().get_align() != 0))
            throw Exception::SharedAlign{};
        if (unlikely(assert_mode && reinterpret_cast<uintptr_t>(address) % alignof(Type) != 0))
            throw Exception::SharedAlign{};
    }
public:
    /** Get the address in shared memory.
     * @return Address in shared memory
    **/
    auto get() const noexcept {
        return address;
    }
public:
    /** Read operation.
     * @param index Index to read
     * @return Private copy of the content at the shared address
    **/
    Type read(size_t index) const {
        if (unlikely(assert_mode && index >= n))
            throw Exception::SharedOverflow{};
        Type res;
        tx.read(address + index, sizeof(Type), &res);
        return res;
    }
    /** Write operation.
     * @param index  Index to write
     * @param source Private content to write at the shared address
    **/
    void write(size_t index, Type const& source) const {
        if (unlikely(assert_mode && index >= n))
            throw Exception::SharedOverflow{};
        tx.write(tx, &source, sizeof(Type), address + index);
    }
public:
    /** Reference a cell.
     * @param index Cell to reference
     * @return Shared on that cell
    **/
    Shared<Type> operator[](size_t index) const {
        if (unlikely(assert_mode && index >= n))
            throw Exception::SharedOverflow{};
        return Shared<Type>{tx, address + index};
    }
    /** Address of the first byte after the array.
     * @return First byte after the array
    **/
    void* after() const noexcept {
        return address + n;
    }
};

// -------------------------------------------------------------------------- //

/** Repeat a given transaction until it commits.
 * @param tm   Transactional memory
 * @param mode Transactional mode
 * @param func Transaction closure (Transaction& -> ...)
 * @return Returned value (or void) when the transaction committed
**/
template<class Func> static auto transactional(TransactionalMemory const& tm, Transaction::Mode mode, Func&& func) {
    do {
        try {
            Transaction tx{tm, mode};
            return func(tx);
        } catch (Exception::TransactionRetry const&) {
            continue;
        }
    } while (true);
}

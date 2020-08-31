/**
 * @file   common.hpp
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
 * Common set of helper functions/classes.
**/

#pragma once

// External headers
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <mutex>
#include <thread>
#include <utility>
extern "C" {
#include <time.h>
}

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

// -------------------------------------------------------------------------- //

// Whether to enable more safety checks
constexpr static auto assert_mode = false;

// Maximum waiting time for initialization/clean-ups (in ms)
constexpr static auto max_side_time = ::std::chrono::milliseconds{2000};

// -------------------------------------------------------------------------- //
namespace Exception {

/** Defines a simple exception.
 * @param name   Exception name
 * @param parent Parent exception (use 'Any' as the root)
 * @param deftxt Default explanatory string
**/
#define EXCEPTION(name, parent, deftxt) \
    class name: public parent { \
    public: \
        /** Default explanatory string constructor. \
        **/ \
        name(): parent{deftxt} {} \
        /** Non-default parent forwarding constructor. \
         * @param ... Forwarded arguments \
        **/ \
        template<class... Args> name(Args&&... args): parent{::std::forward<Args>(args)...} {} \
    }

/** Root user exception class.
**/
class Any: public ::std::exception {
protected:
    char const* text; // Explanation string
public:
    /** Explanatory string constructor.
     * @param text Explanatory null-terminated string
    **/
    Any(char const* text): text{text} {}
public:
    /** Return the explanatory string.
     * @return Explanatory string
    **/
    virtual char const* what() const noexcept {
        return text;
    }
};

/** Exception tree.
**/
EXCEPTION(Unreachable, Any, "unreachable code reached");
EXCEPTION(Bounded, Any, "bounded execution exception");
    EXCEPTION(BoundedOverrun, Any, "bounded execution overrun");

}
// -------------------------------------------------------------------------- //

/** Non-copyable helper base class.
**/
class NonCopyable {
public:
    /** Deleted copy constructor/assignment.
    **/
    NonCopyable(NonCopyable const&) = delete;
    NonCopyable& operator=(NonCopyable const&) = delete;
protected:
    /** Protected default constructor, to make sure class is not directly instantiated.
    **/
    NonCopyable() = default;
};

/** Time accounting class.
**/
class Chrono final {
public:
    /** Tick class (always 1 tick = 1 ns).
    **/
    using Tick = uint_fast64_t;
    constexpr static auto invalid_tick = Tick{0xbadc0de}; // Invalid tick value
private:
    Tick total; // Total tick counter
    Tick local; // Segment tick counter
public:
    /** Tick constructor.
     * @param tick Initial number of ticks (optional)
    **/
    Chrono(Tick tick = 0) noexcept: total{tick} {}
private:
    /** Call a "clock" function, convert the result to the Tick type.
     * @param func "Clock" function to call
     * @return Resulting time
    **/
    static Tick convert(int (*func)(::clockid_t, struct ::timespec*)) noexcept {
        struct ::timespec buf;
        if (unlikely(func(CLOCK_MONOTONIC, &buf) < 0))
            return invalid_tick;
        auto res = static_cast<Tick>(buf.tv_nsec) + static_cast<Tick>(buf.tv_sec) * static_cast<Tick>(1000000000ul);
        if (unlikely(res == invalid_tick)) // Bad luck...
            return invalid_tick + 1;
        return res;
    }
public:
    /** Get the resolution of the clock used.
     * @return Resolution (in ns), 'invalid_tick' for unknown
    **/
    static auto get_resolution() noexcept {
        return convert(::clock_getres);
    }
public:
    /** Start measuring a time segment.
    **/
    void start() noexcept {
        local = convert(::clock_gettime);
    }
    /** Measure a time segment.
    **/
    auto delta() noexcept {
        return convert(::clock_gettime) - local;
    }
    /** Stop measuring a time segment, and add it to the total.
    **/
    void stop() noexcept {
        total += delta();
    }
    /** Reset the total tick counter.
    **/
    void reset() noexcept {
        total = 0;
    }
    /** Get the total tick counter.
     * @return Total tick counter
    **/
    auto get_tick() const noexcept {
        return total;
    }
};

/** Atomic waitable latch class.
**/
class Latch final {
private:
    ::std::mutex            lock; // Local lock
    ::std::condition_variable cv; // For waiting/waking up
    bool                  raised; // State of the latch
public:
    /** Deleted copy/move constructor/assignment.
    **/
    Latch(Latch const&) = delete;
    Latch& operator=(Latch const&) = delete;
    /** Initial state constructor.
     * @param raised Initial state of the latch
    **/
    Latch(bool raised = false): lock{}, raised{raised} {}
public:
    /** Raise the latch, no-op if already raised, release semantic.
    **/
    void raise() {
        // Release fence
        ::std::atomic_thread_fence(::std::memory_order_release);
        // Raise
        ::std::unique_lock<decltype(lock)> guard{lock};
        raised = true;
        cv.notify_all();
    }
    /** Wait for the latch to be raised, then reset it, acquire semantic if no timeout.
     * @param maxtick Maximal duration to wait for (in ticks)
     * @return Whether the latch was raised before the maximal duration elapsed
    **/
    bool wait(Chrono::Tick maxtick) {
        { // Wait for raised
            ::std::unique_lock<decltype(lock)> guard{lock};
            // Wait
            if (maxtick == Chrono::invalid_tick) {
                while (!raised)
                    cv.wait(guard);
            } else {
                if (!cv.wait_for(guard, ::std::chrono::nanoseconds{maxtick}, [&]() { return raised; })) // Overtime
                    return false;
            }
            // Reset
            raised = false;
        }
        // Acquire fence
        ::std::atomic_thread_fence(::std::memory_order_acquire);
        return true;
    }
};

// -------------------------------------------------------------------------- //

/** Pause execution for a "short" period of time.
**/
static void short_pause() {
#if (defined(__i386__) || defined(__x86_64__)) && defined(USE_MM_PAUSE)
    _mm_pause();
#else
    ::std::this_thread::yield();
#endif
}

/** Run some function for some bounded time, throws 'Exception::BoundedOverrun' on overtime.
 * @param dur  Maximum execution duration
 * @param func Function to run (void -> void)
 * @param emsg Null-terminated error message
**/
template<class Rep, class Period, class Func> static void bounded_run(::std::chrono::duration<Rep, Period> const& dur, Func&& func, char const* emsg) {
    ::std::mutex lock;
    ::std::unique_lock<decltype(lock)> guard{lock};
    ::std::condition_variable cv;
    ::std::thread runner{[&]() {
        func();
        { // Notify master
            ::std::unique_lock<decltype(lock)> guard{lock};
            cv.notify_all();
        }
    }};
    if (cv.wait_for(guard, dur) != std::cv_status::no_timeout) {
        runner.detach();
        throw Exception::BoundedOverrun{emsg};
    }
    runner.join();
}

/** Spin barrier class.
**/
class Barrier final {
public:
    /** Counter class.
    **/
    using Counter = uint_fast32_t;
    /** Mode enum class.
    **/
    enum class Mode {
        enter,
        leave
    };
private:
    Counter cardinal; // Total number of threads that synchronize
    ::std::atomic<Counter> mutable step; // Step counters
    ::std::atomic<Mode>    mutable mode; // Current mode
public:
    /** Deleted copy constructor/assignment.
    **/
    Barrier(Barrier const&) = delete;
    Barrier& operator=(Barrier const&) = delete;
    /** Number of threads constructor.
     * @param cardinal Non-null total number of threads synchronizing on this barrier
    **/
    Barrier(Counter cardinal): cardinal{cardinal}, step{0}, mode{Mode::enter} {}
public:
    /** [thread-safe] Synchronize all the threads.
    **/
    void sync() const {
        // Enter
        if (step.fetch_add(1, ::std::memory_order_relaxed) + 1 == cardinal) { // Set leave mode
            mode.store(Mode::leave, ::std::memory_order_release);
        } else { // Wait for leave mode
            while (unlikely(mode.load(::std::memory_order_acquire) != Mode::leave))
                short_pause();
        }
        // Leave
        if (step.fetch_sub(1, ::std::memory_order_relaxed) - 1 == 0) { // Set enter mode
            mode.store(Mode::enter, ::std::memory_order_release);
        } else { // Wait for enter mode
            while (unlikely(mode.load(::std::memory_order_acquire) != Mode::enter))
                short_pause();
        }
    }
};

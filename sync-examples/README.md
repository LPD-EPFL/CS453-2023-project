# Simple synchronization examples.

Modifying a variable that is concurrently accessed by another thread is
dangerous and will (most of the time) lead to Undefined Behaviors and Data
Races.

Through simple examples, we'll see how to use synchronization primitives to
build thread-safe programs using locks and atomic variables.

We build 3 examples:
- A counter. Each thread increments a shared counter repeatedly RUNS times.
We check that the counter = RUNS * THREADS at the end.
- Leader election. In each run, one thread is elected as the leader. We check
that exactly one thread considers itself as the leader in each run.
- Producer-Consumer. One thread generates data in a circular buffer while the
second tries to consume it. They have to be kept in sync.

These examples also show how to start threads and wait for their completion.
YOU DON'T HAVE TO START THREADS YOURSELVES IN THE DUAL-VERSIONED STM ALGO.
In the STM, threads are started by the users of your library (in which they
repeatedly (1) start a transaction, (2) read/write/alloc/free STM memory and (3)
commit the transaction.).

## Counter

### Bad approach #1
We employ no synchronization primitive: Undefined Behavior (UB), bad bad.

### Good approach #1
We take a big nice lock around the ++: correct.

### Bad approach #2
We use an atomic variable but use 2 non atomic operations: not UB, but doesn't
work.

### Good approach #2
We use an atomic variable and an atomic operation (fetch and add): correct.

## Leader election

### Bad approch # 1
We employ no synchronization primitive: Undefined Behavior (UB), bad bad.

### Good approach #1
We take a big nice lock around the test and set: correct.

### Bad approach #2
We use an atomic variable but use 2 non atomic operations: not UB, but doesn't
work.

### Good approach #2
We use an atomic variable and an atomic operation (compare and swap): correct.

## Producer-consumer

### Bad approach
We employ no synchronization primitive: Undefined Behavior (UB), bad bad.

### Okayish approach #1
We take a big nice lock when when writting/reading and checking the bounds:
correct.

### Okayish approach #2
We use atomic variables and fences: correct.

### Good approach
We use a conditional variable. :)
https://www.ibm.com/docs/en/i/7.1?topic=ssw_ibm_i_71/apis/users_78.htm
Conditional variables are synchronization primitives provided by the kernel
that let a thread sleep until it's woken up. Using a "cond var", a consummer
that realizes that data has not been generated yet can go to sleep instead of
busy waiting. It will then be woken up by the producer once the data is
generated. :)

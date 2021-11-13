#include <pthread.h>
#include <inttypes.h>
#include <assert.h>
#include <stdio.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <stdbool.h>

#include "lock.h"

#define RUNS 4096
#define THREADS 4
#define DATA_TEXT_SIZE 1024
#define BUFFER_SIZE 8

struct data {
  char text[DATA_TEXT_SIZE];
};

static struct lock_t lock;

bool are_same(struct data* a, struct data* b) {
  for (int i = 0; i < DATA_TEXT_SIZE; i++)
    if (a->text[i] != b->text[i]) return false;
  return true;
}

struct data buffer[BUFFER_SIZE] = { 0 };

int produced_until = 0;
int consumed_until = 0;

struct data produced[RUNS] = { 0 }; // used to check correctness
struct data consumed[RUNS] = { 0 }; // used to check correctness

void* produce(void* null) {
  for (int r = 0; r < RUNS; r++) {
    lock_acquire(&lock);
    while (true) {
      if (consumed_until + BUFFER_SIZE > r) break;
      lock_wait(&lock);
      // Note: waiting releases the lock and puts the thread to sleep. Once
      // woken up, it will need to acquire back the lock to continue its
      // execution.
      // Said differently, if multiple threads are waiting, upon "lock_wake_up",
      // only one of them will continue the execution at a time (because the
      // lock is exclusive).
    }
    printf("can produce %d\n", r);
    for (int i = 0; i < DATA_TEXT_SIZE; i++)
      produced[r].text[i] = rand();
    buffer[r % BUFFER_SIZE] = produced[r];
    produced_until++;
    lock_release(&lock);
    lock_wake_up(&lock); // We tell the consumer it can continue consuming.
    // Correct: This is a better version of the lock one in which we do not busy
    // wait but rely on a notification primitive to let the "idle" cores rest.
  }
}

void* consume(void* null) {
  for (int r = 0; r < RUNS; r++) {
    lock_acquire(&lock);
    while (true) {
      if (produced_until > r) break;
      lock_wait(&lock);
    }
    printf("can consume %d\n", r);
    consumed[r] = buffer[r % BUFFER_SIZE];
    consumed_until++;
    lock_release(&lock);
    lock_wake_up(&lock); // We tell the producer it can continue producing.
  }
}

int main() {
  lock_init(&lock);
  int res;
  pthread_t producer;
  res = pthread_create(&producer, NULL, produce, NULL);
  assert(!res);

  pthread_t consumer;
  res = pthread_create(&consumer, NULL, consume, NULL);
  assert(!res);

  res = pthread_join(consumer, NULL);
  assert(!res);

  res = pthread_join(producer, NULL);
  assert(!res);

  int r = 0;
  for (; r < RUNS; r++) {
    if (!are_same(&produced[r], &consumed[r])) {
      printf("Consumed the wrong data on round %d.\n", r);
      break;
    }
  }
  if (r == RUNS) {
    printf("Looks correct to me! :)\n");
  }
}

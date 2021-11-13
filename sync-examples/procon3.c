#include <pthread.h>
#include <inttypes.h>
#include <assert.h>
#include <stdio.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <stdbool.h>

#define RUNS 4096
#define THREADS 4
#define DATA_TEXT_SIZE 1024
#define BUFFER_SIZE 1024

struct data {
  char text[DATA_TEXT_SIZE];
};

bool are_same(struct data* a, struct data* b) {
  for (int i = 0; i < DATA_TEXT_SIZE; i++)
    if (a->text[i] != b->text[i]) return false;
  return true;
}

struct data buffer[BUFFER_SIZE] = { 0 };

atomic_int produced_until = 0;
atomic_int consumed_until = 0;

struct data produced[RUNS] = { 0 }; // used to check correctness
struct data consumed[RUNS] = { 0 }; // used to check correctness

void* produce(void* null) {
  for (int r = 0; r < RUNS; r++) {
    while (true) {
      // The memory order can be relaxed as we don't read anything "produced"
      // by the consumer.
      int local_cu = atomic_load_explicit(&consumed_until, memory_order_relaxed);
      if (local_cu + BUFFER_SIZE > r) break;
    }
    printf("can produce %d\n", r);
    for (int i = 0; i < DATA_TEXT_SIZE; i++)
      produced[r].text[i] = rand();
    buffer[r % BUFFER_SIZE] = produced[r];
    // We want to increment "produced_until" after the buffer has been written.
    // By using memory_order_release, we prevent the STOREs on buffer from being
    // reordered after the atomic operation.
    atomic_fetch_add_explicit(&produced_until, 1, memory_order_release);
  }
}

void* consume(void* null) {
  for (int r = 0; r < RUNS; r++) {
    while (true) {
      // We don't want to access the buffer before checking the atomic variable.
      // The memory_order_acquire prevents this reordering.
      int local_pu = atomic_load_explicit(&produced_until, memory_order_acquire);
      if (local_pu > r) break;
    }
    printf("can consume %d\n", r);
    consumed[r] = buffer[r % BUFFER_SIZE];
    atomic_fetch_add_explicit(&consumed_until, 1, memory_order_release);
  }
}

int main() {
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

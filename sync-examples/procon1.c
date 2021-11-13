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

struct data produced[RUNS] = { 0 }; // used to check correctness
struct data consumed[RUNS] = { 0 }; // used to check correctness

void* produce(void* null) {
  for (int r = 0; r < RUNS; r++) {    
    for (int i = 0; i < DATA_TEXT_SIZE; i++)
      produced[r].text[i] = rand();
    buffer[r % BUFFER_SIZE] = produced[r];
    // Super wrong: Overwritting data being read by the consumer.
  }
}

void* consume(void* null) {
  for (int r = 0; r < RUNS; r++) {
    consumed[r] = buffer[r % BUFFER_SIZE];
    // Super wrong: Reading data concurrently (or not even) written by the
    // producer.
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

#include <pthread.h>
#include <inttypes.h>
#include <assert.h>
#include <stdio.h>
#include <stdatomic.h>

#define RUNS (4096 * 256)
#define THREADS 4

static atomic_int counter = 0;

void* count(void* null) {
  for (int r = 0; r < RUNS; r++) {
    int read_copy = counter;
    counter = read_copy + 1;
    // **Incorrect**, using atomic variables (~atomic registers is not enough).
  }
}

int main() {
  pthread_t handlers[THREADS];

  for (intptr_t i = 0; i < THREADS; i++) {
    int res = pthread_create(&handlers[i], NULL, count, NULL);
    assert(!res);
  }

  for (int i = 0; i < THREADS; i++) {
    int res = pthread_join(handlers[i], NULL);
    assert(!res);
  }

  if (counter != RUNS * THREADS) {
    printf("Didn't count so well. :/, found %d\n", counter);
  } else {
    printf("Counted up to %d.\n", counter);
  }
}

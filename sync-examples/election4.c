#include <pthread.h>
#include <inttypes.h>
#include <assert.h>
#include <stdio.h>
#include <stdatomic.h>

#define RUNS (4096 * 256)
#define THREADS 4

static atomic_int leader[RUNS] = { 0 };
static atomic_int nb_leaders[RUNS] = { 0 }; // used to check correctness

void* elect(void* _tid) {
  intptr_t tid = (intptr_t) _tid;
  for (int r = 0; r < RUNS; r++) {
    int expected = 0;
    if (atomic_compare_exchange_strong(&leader[r], &expected, tid)) {
      atomic_fetch_add(&nb_leaders[r], 1); // used to check correctness
    }
    // **Correct**, we use the Compare and Swap operation to atomically check if
    // a leader has already been elected and if not, we impose ourselves. :)
  }
}

int main() {
  pthread_t handlers[THREADS];

  for (intptr_t i = 0; i < THREADS; i++) {
    int res = pthread_create(&handlers[i], NULL, elect, (void*)(i + 1));
    assert(!res);
  }

  for (int i = 0; i < THREADS; i++) {
    int res = pthread_join(handlers[i], NULL);
    assert(!res);
  }

  int r = 0;
  for (; r < RUNS; r++) {
    if (nb_leaders[r] != 1) {
      printf("Leader election for round %d failed.\n", r);
      break;
    }
  }
  if (r == RUNS) {
    printf("Looks correct to me! :)\n");
  }
}

//; RUN: %sea "%s" --own-sem 2>&1 | OutputCheck %s
// CHECK: ^unsat$
#include "seahorn/seahorn.h"
#include <stdbool.h>
#include <stdlib.h>

extern char nd_char();

extern char *__sea_copy_extptr_slots_hm(char *dst, char *src);
typedef struct handle_t {
  unsigned val;
  bool valid;
} Handle;

int main() {
  sea_tracking_on();
  char *a, *b, *c;
  Handle *h1 = (Handle *)malloc(sizeof(Handle));
  Handle *h2 = (Handle *)malloc(sizeof(Handle));
  h1->val = 0;
  h1->valid = true;
  h2->val = 1;
  h2->valid = false;
  Handle *h1b, *h1b1, *h1d;
  SEA_LOAD_CACHE_AND_BORROW(h1d, h1b, h1, h1->valid);

  // When writing to memory, also write to cache.
  SEA_WRITE_CACHE(h1b1, h1b, false);
  h1b1->valid = false;

  SEA_DIE(h1b1);

  // It is valid to read from cache instead of memory
  // since h11u is unique;
  bool v;
  SEA_READ_CACHE(v, (char *)h1d);
  sassert(v == false);

  return 0;
}

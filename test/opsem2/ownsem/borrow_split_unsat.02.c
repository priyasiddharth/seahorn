//; RUN: %sea "%s" --own-sem 2>&1 | OutputCheck %s
// CHECK: ^unsat$
#include "seahorn/seahorn.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

extern char nd_char();
extern bool nd_bool();

typedef struct handle_t {
  unsigned val;
  bool valid;
} Handle;

int main() {
  sea_tracking_on();

  Handle *h1, *h00, *h0;
  Handle *h000 = (Handle *)malloc(sizeof(Handle));
  h000->val = 0;
  h000->valid = false;

  SEA_MKOWN(h00, h000);

  SEA_WRITE_CACHE(h0, h00, false);

  // No need to set fatptr_slot1
  // A correct non deterministic value is read from slot1
  // SEA_SET_FATPTR_SLOT1(h0, h00, 0xA);
  bool *h0b0_valid, *h0b1_valid;
  SEA_BORROW_OFFSET(h1, h0b0_valid, h0, offsetof(Handle, valid));

  // write to cache and mem
  SEA_WRITE_CACHE(h0b1_valid, h0b0_valid, true);
  *h0b1_valid = true;

  SEA_DIE(h0b1_valid);

  bool valToAssert;
  SEA_READ_CACHE(valToAssert, (char *)h1);

  sassert(valToAssert == true);
  Handle *h1s, *h1s1, *h1o, *h1o1;
  Handle *h1b, *h1b1, *h1b2, *h1d;
  if (nd_bool()) {
    // non deterministically choose to have split-borrowed
    // access on h1
    bool cacheVal;
    SEA_READ_CACHE(cacheVal, (char *)h1);
    h1->valid = cacheVal; // write cache to mem
    SEA_MKSHR(h1s1, h1);
    h1s1->val = 1;
    h1s1->valid = false;
    SEA_MKOWN(h1o, h1s1);
    SEA_WRITE_CACHE(h1o1, h1o, h1o->valid);
    SEA_READ_CACHE(valToAssert, (char *)h1o1);
    sassert(valToAssert == false);
    SEA_BORROW(h1d, h1b, h1o1);
  } else {
    SEA_BORROW(h1d, h1b, h1);
  }

  bool *h1b_valid, *h2b_valid;
  SEA_BORROW_OFFSET(h1b2, h1b_valid, h1b, offsetof(Handle, valid));
  // When writing to memory, also write to cache.
  SEA_WRITE_CACHE(h2b_valid, h1b_valid, false);
  *h2b_valid = false;
  SEA_DIE(h2b_valid);

  // It is valid to read from cache instead of memory
  // since h11u is unique;
  bool v;
  SEA_READ_CACHE(v, (char *)h1d);
  sassert(v == false);

  return 0;
}

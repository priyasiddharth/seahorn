//; RUN: %sea "%s" --own-sem 2>&1 | OutputCheck %s
// CHECK: ^unsat$
#include "seahorn/seahorn.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

extern char nd_char();
extern void pretendEscapeToMemory(char *);

typedef struct handle_t {
  unsigned val;
  bool valid;
} Handle;

int main() {
  sea_tracking_on();
  char *a, *b, *c;

  Handle *h1, *h0, *h00, *h000;
  Handle *h0000 = (Handle *)malloc(sizeof(Handle));
  h0000->val = 0;
  h0000->valid = false;

  SEA_MKOWN(h000, h0000);

  SEA_WRITE_CACHE(h00, h000, false);
  // No need to set fatptr_slot1
  // A correct non deterministic value is read from slot1
  // SEA_SET_FATPTR_SLOT1(h0, h00, 0xA);
  bool *h0b0_valid, *h0b1_valid;
  SEA_BORROW_OFFSET(h1, h0b0_valid, h0, offsetof(Handle, valid));

  pretendEscapeToMemory(h0b1_valid);

  // write to cache and mem
  SEA_WRITE_CACHE(h0b1_valid, h0b0_valid, true);
  *h0b1_valid = true;

  SEA_DIE(h0b1_valid);
  bool valToAssert;
  SEA_READ_CACHE(valToAssert, (char *)h1);
  sassert(valToAssert == true);
  return 0;
}

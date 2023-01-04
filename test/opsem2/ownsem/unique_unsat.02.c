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
  Handle *h1u, *h1s;
  SEA_LOAD_CACHE_AND_BEGIN_UNIQUE(h1u, h1, h1->valid);

  h1u->valid = false;

  SEA_UNLOAD_CACHE_AND_END_UNIQUE(h1s, (char *)h1u, &h1s->valid, 1);
  sassert(__sea_get_extptr_slot0_hm((char *)h1s) == false);
  return 0;
}

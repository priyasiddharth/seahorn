//; RUN: %sea "%s" --horn-vcgen-use-ite --horn-vcgen-only-dataflow --horn-bmc-coi --own-sem 2>&1 | OutputCheck %s
// CHECK: ^unsat$
#include "seahorn/seahorn.h"
#include <stdlib.h>

extern char nd_char();

int main() {
  char *a, *b, *c;
  a = malloc(1024);
  SEA_BEGIN_UNIQUE(b, a);
  *b = nd_char();
  if (nd_char()) {
    assume(*b > 0 && *b < 5);
  } else {
    assume(*b <= 0);
  }
  SEA_END_UNIQUE(c, b);
  sassert(*c < 5);
  return 0;
}

//; RUN: %sea "%s" --own-sem 2>&1 | OutputCheck %s
// CHECK: ^unsat$
#include "seahorn/seahorn.h"
#include <stdlib.h>

extern char *sea_begin_unique(char *);
extern char *sea_end_unique(char *);
extern char nd_char();
 
int main() {
  sea_tracking_on();
  char *a, *b, *c;
  a = malloc(1024);
  SEA_BEGIN_UNIQUE(b, a);
  *b = nd_char();
  assume(*b < 5);
  SEA_END_UNIQUE(c, b);
  sassert(*c < 5);
  return 0;
}

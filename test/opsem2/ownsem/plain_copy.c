//; RUN: %sea "%s" --own-sem  --horn-bv2-extra-widemem --horn-bv2-tracking-mem 2>&1 | OutputCheck %s
// CHECK: ^unsat$
#include "seahorn/seahorn.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>

extern void sea_set_shadowmem(char, char*, char);
extern char sea_get_shadowmem(char, char*);
extern void sea_tracking_on();
extern void sea_tracking_off();
extern char nd_char();
extern void pretendEscapeToMemory(char *);
extern bool nd_bool(void);

int main() {
  char *p = (char *)malloc(sizeof(char));
  char c = nd_char();
  assume(c == 0);
  *p = c;
  char *b = p;
  if (nd_bool()) {
    char v1 = nd_char();
    assume(v1 > 1);
    *b = v1;
    pretendEscapeToMemory(b);
  }
  sassert(*p == 0 || *p > 1);
  return 0;
}

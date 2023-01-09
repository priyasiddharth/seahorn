#ifndef _SEAHORN__H_
#define _SEAHORN__H_

#include <seadsa/sea_dsa.h>

#include <stdbool.h>
#include <stdint.h>

#define SEA_NONDET_FN_ATTR __declspec(noalias)

#ifdef __cplusplus
extern "C" {
#endif
/**
   Marks an error location for the verifier

   Catastrophic failure that matters.
 */
extern void __VERIFIER_error(void);

/**
 A condition to be assumed to be true by the verifier

*/
extern void __VERIFIER_assume(int);
extern void __SEA_assume(bool);

extern void __VERIFIER_assert(bool);
extern void __VERIFIER_assert_not(bool);
extern void __VERIFIER_assert_if(bool, bool);
/**
   Returns TRUE if \p offset number of bytes of \p ptr are allocated

    Requires support from the memory manager. Might be interpreted to always
    return TRUE or FALSE if the memory manager does not support it.
 */
#define sea_is_deref sea_is_dereferenceable
extern bool sea_is_dereferenceable(const void *ptr, intptr_t offset);
extern void sea_assert_if(bool, bool);
/* returns true if memory pointed to by arg has been modified from
 * 1. allocation OR
 * 2. reset_modified OR
 * 3. sea_tracking_on
 * whichever is the latest event.
 */
extern bool sea_is_modified(char *);
/* tracking is set to on for subsequent program till exit or sea_tracking_off */
extern void sea_tracking_on(void);
/* tracking is set to off for subsequent program till exit or sea_tracking_on */
extern void sea_tracking_off(void);
/* reset modified metadata for memory pointed to by arg */
extern void sea_reset_modified(char *);
/* Set a shadow memory slot S at addr A with value V.
 * arg0 - S. Note that 0 is main memory and should not be used.
 * arg1 - A
 * arg2 - V
 */
extern void sea_set_shadowmem(char, char *, char);
/* Get a value from shadow memory slot S at address A.
 * arg0 - S. Note that 0 is main memory and should not be used.
 * arg1 - A
 */
extern char sea_get_shadowmem(char, char *);
#ifdef __cplusplus
}
#endif

extern char *sea_begin_unique(char *);
extern char *sea_end_unique(char *);
extern char *__sea_set_extptr_slot0_hm(char *ptr, char val);
extern char __sea_get_extptr_slot0_hm(char *ptr);
extern char *__sea_set_extptr_slot1_hm(char *ptr, char val);
extern char __sea_get_extptr_slot1_hm(char *ptr);
extern char nd_char();
extern char *sea_bor_mkbor(char *);
extern char *sea_bor_mksuc(char *);
extern void sea_die(char *);
#define SEA_BEGIN_UNIQUE(DST, SRC)                                             \
  do {                                                                         \
    (DST) = sea_begin_unique((char *)(SRC));                                   \
    sea_dsa_alias((DST), (SRC));                                               \
  } while (0)

#define SEA_END_UNIQUE(DST, SRC)                                               \
  do {                                                                         \
    (DST) = sea_end_unique((char *)(SRC));                                     \
    sea_dsa_alias((DST), (SRC));                                               \
  } while (0)

#define SEA_LOAD_CACHE_AND_BEGIN_UNIQUE(DST, SRC, VAL)                         \
  do {                                                                         \
    char *intmd = __sea_set_extptr_slot0_hm((char *)SRC, (char)VAL);           \
    sea_dsa_alias(intmd, SRC);                                                 \
    (DST) = sea_begin_unique((char *)intmd);                                   \
    sea_dsa_alias((DST), (intmd));                                             \
  } while (0)

// TODO: make cache nd after unloading
#define SEA_UNLOAD_CACHE_AND_END_UNIQUE(DST, SRC, DSTADDRESS, DSTLEN)          \
  do {                                                                         \
    char uniqval = __sea_get_extptr_slot0_hm((char *)SRC);                     \
    (DST) = sea_end_unique((char *)(SRC));                                     \
    memset((char *)DSTADDRESS, uniqval, 1 /* FIXME: use DSTLEN */);            \
    sea_dsa_alias((DST), (SRC));                                               \
  } while (0)

#define SEA_WRITE_CACHE(DST, SRC, VAL)                                         \
  do {                                                                         \
    (DST) = __sea_set_extptr_slot0_hm((char *)(SRC), (char)(VAL));             \
    sea_dsa_alias((DST), (SRC));                                               \
  } while (0)

#define SEA_READ_CACHE(VAL, SRC)                                               \
  do {                                                                         \
    (VAL) = __sea_get_extptr_slot0_hm((char *)SRC);                            \
  } while (0)

#define SEA_BORROW(DST, BOR, SRC)                                              \
  do {                                                                         \
    char *bor_intmd0 = sea_bor_mkbor((char *)SRC);                             \
    char *suc_intmd0 = sea_bor_mksuc((char *)SRC);                             \
    char brval = __sea_get_extptr_slot0_hm((char *)SRC);                       \
    char *bor_intmd1 = __sea_set_extptr_slot0_hm(bor_intmd0, brval);           \
    char ndval = nd_char();                                                    \
    (BOR) = __sea_set_extptr_slot1_hm(bor_intmd1, ndval);                      \
    (DST) = __sea_set_extptr_slot0_hm(suc_intmd0, ndval);                      \
    sea_dsa_alias((BOR), (SRC));                                               \
    sea_dsa_alias((DST), (SRC));                                               \
    /* TODO: remove if sea_dsa_alias is transitive */                          \
    sea_dsa_alias((DST), (BOR));                                               \
  } while (0)

#define SEA_DIE(SRC)                                                           \
  do {                                                                         \
    char nd_retval = __sea_get_extptr_slot1_hm((char *)(SRC));                 \
    char cacheval = __sea_get_extptr_slot0_hm((char *)(SRC));                  \
    assume(nd_retval == cacheval);                                             \
    sea_die((char *)(SRC));                                                    \
  } while (0)

#define SEA_LOAD_CACHE_AND_BORROW(DST, BOR, SRC, VAL)                          \
  do {                                                                         \
    char *intmd = __sea_set_extptr_slot0_hm((char *)(SRC), (char)(VAL));       \
    sea_dsa_alias(intmd, (SRC));                                               \
    SEA_BORROW((DST), (BOR), intmd);                                           \
  } while (0)

/* Convenience macros */
#define assume __SEA_assume

#ifdef VACCHECK
/* See https://github.com/seahorn/seahorn/projects/5 for details */
#define sassert(X)                                                             \
  (void)((__VERIFIER_assert(X), (X)) || (__VERIFIER_error(), 0))
#elif defined(SEA_SYNTH)
/* See test/synth/ for use cases */
#define PARTIAL_FN                                                             \
  __attribute__((annotate("partial"))) __attribute__((noinline))
#define sassert(X)                                                             \
  (void)((__VERIFIER_assert(X), (X)) || (__VERIFIER_error(), 0))
#else
/* Default semantics of sassert */
#define sassert(X) (void)((X) || (__VERIFIER_error(), 0))
#endif

#endif

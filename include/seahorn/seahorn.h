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
extern char *sea_set_fatptr_slot(char *ptr, char slot, uint64_t val);
extern uint64_t sea_get_fatptr_slot(char *ptr, char slot);
extern char nd_char();
extern uint64_t nd_uint64t();
extern char *sea_bor_mkbor(char *);
extern char *sea_bor_mksuc(char *);
extern void sea_die(char *);
extern char *sea_mkown(char *);
extern void sea_bor_ptr(char *);

#define SEA_SET_FATPTR_SLOT0(DST, SRC, VAL)                                    \
  do {                                                                         \
    (DST) = sea_set_fatptr_slot((char *)(SRC), 0, (uint64_t)VAL);              \
  } while (0);

#define SEA_SET_FATPTR_SLOT1(DST, SRC, VAL)                                    \
  do {                                                                         \
    (DST) = sea_set_fatptr_slot((char *)(SRC), 1, (uint64_t)VAL);              \
  } while (0);

#define SEA_GET_FATPTR_SLOT0(SRC, VAL)                                         \
  do {                                                                         \
    (VAL) = sea_get_fatptr_slot((char *)(SRC), 0);                             \
  } while (0);

#define SEA_GET_FATPTR_SLOT1(SRC, VAL)                                         \
  do {                                                                         \
    (VAL) = sea_get_fatptr_slot((char *)(SRC), 1);                             \
  } while (0);

#define SEA_MKOWN(DST, SRC)                                                    \
  do {                                                                         \
    (DST) = sea_mkown((char *)(SRC));                                          \
    sea_dsa_alias((DST), (SRC));                                               \
  } while (0)

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
    char *intmd = __sea_set_extptr_slot0_hm((char *)SRC, VAL);                 \
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
    SEA_SET_FATPTR_SLOT0((DST), (char *)(SRC), (VAL));                         \
    sea_dsa_alias((DST), (SRC));                                               \
  } while (0)

#define SEA_READ_CACHE(VAL, SRC)                                               \
  do {                                                                         \
    SEA_GET_FATPTR_SLOT0((char *)SRC, (VAL));                                  \
  } while (0)

#define SEA_BORROW(DST, BOR, SRC)                                              \
  do {                                                                         \
    char *bor_intmd0 = sea_bor_mkbor((char *)SRC);                             \
    char *suc_intmd0 = sea_bor_mksuc((char *)SRC);                             \
    uint64_t brval;                                                            \
    SEA_GET_FATPTR_SLOT0(SRC, brval);                                          \
    uint64_t retval;                                                           \
    SEA_GET_FATPTR_SLOT1(SRC, retval);                                         \
    char *bor_intmd1;                                                          \
    SEA_SET_FATPTR_SLOT0(bor_intmd1, bor_intmd0, brval);                       \
    uint64_t ndval = nd_uint64t();                                             \
    assume(ndval == retval);                                                   \
    SEA_SET_FATPTR_SLOT1(BOR, bor_intmd1, ndval)                               \
    SEA_SET_FATPTR_SLOT0(DST, suc_intmd0, ndval)                               \
    sea_dsa_alias((BOR), (SRC));                                               \
    sea_dsa_alias((DST), (SRC));                                               \
    /* TODO: remove if sea_dsa_alias is transitive */                          \
    sea_dsa_alias((DST), (BOR));                                               \
  } while (0)

#define SEA_DIE(SRC)                                                           \
  do {                                                                         \
    uint64_t nd_retval;                                                        \
    SEA_GET_FATPTR_SLOT1((char *)(SRC), nd_retval);                            \
    uint64_t cacheval;                                                         \
    SEA_GET_FATPTR_SLOT0((char *)(SRC), cacheval);                             \
    assume(nd_retval == cacheval);                                             \
    sea_die((char *)(SRC));                                                    \
  } while (0)

#define SEA_LOAD_CACHE_AND_BORROW(DST, BOR, SRC, VAL)                          \
  do {                                                                         \
    char *intmd = __sea_set_extptr_slot0_hm((char *)(SRC), (VAL));             \
    sea_dsa_alias(intmd, (SRC));                                               \
    SEA_BORROW((DST), (BOR), intmd);                                           \
  } while (0)

#define SEA_BORROW_OFFSET(DST, BOR_OFF, SRC, OFFSET)                           \
  do {                                                                         \
    char *boroff_intmd0;                                                       \
    SEA_BORROW((DST), boroff_intmd0, SRC);                                     \
    sea_bor_ptr((char *)DST);                                                  \
    (BOR_OFF) = (boroff_intmd0) + OFFSET;                                      \
    sea_dsa_alias((BOR_OFF), boroff_intmd0);                                   \
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

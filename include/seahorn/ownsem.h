
#include <stdbool.h>
#include <stdint.h>

extern char *sea_begin_unique(char *);
extern char *sea_end_unique(char *);
extern char *__sea_set_extptr_slot0_hm(char *ptr, char val);
extern char __sea_get_extptr_slot0_hm(char *ptr);
extern char *__sea_set_extptr_slot1_hm(char *ptr, char val);
extern char __sea_get_extptr_slot1_hm(char *ptr);
extern char *sea_set_fatptr_slot(char *ptr, char slot, uint64_t val);
extern uint64_t sea_get_fatptr_slot(char *ptr, char slot);
extern char nd_char(void);
extern uint64_t nd_uint64t(void);
extern char *sea_bor_mkbor(char *);
extern char *sea_bor_mksuc(char *);
extern void sea_die(char *);
extern char *sea_mkown(char *);
extern char *sea_mkshr(char *);
extern char *sea_bor_mem2reg(char *);
extern char *sea_mov_reg2mem(char *);

#define SEA_SET_FATPTR_SLOT0(SRC, VAL)                                         \
  do {                                                                         \
    (SRC) = (typeof(SRC))sea_set_fatptr_slot((char *)(SRC), 0, (uint64_t)VAL); \
  } while (0);

#define SEA_SET_FATPTR_SLOT1(SRC, VAL)                                         \
  do {                                                                         \
    (SRC) = (typeof(SRC))sea_set_fatptr_slot((char *)(SRC), 1, (uint64_t)VAL); \
  } while (0);

#define SEA_GET_FATPTR_SLOT0(SRC, VAL)                                         \
  do {                                                                         \
    (VAL) = (typeof(VAL))sea_get_fatptr_slot((char *)(SRC), 0);                \
  } while (0);

#define SEA_GET_FATPTR_SLOT1(SRC, VAL)                                         \
  do {                                                                         \
    (VAL) = (typeof(VAL))sea_get_fatptr_slot((char *)(SRC), 1);                \
  } while (0);

// NOTE: intrinsic
#define SEA_MKOWN(SRC)                                                         \
  do {                                                                         \
    uint64_t nd_slot0 = nd_uint64t();                                          \
    uint64_t nd_slot1 = nd_uint64t();                                          \
    char *intmd0, *intmd1, *intmd2;                                            \
    intmd0 = sea_mkown((char *)(SRC));                                         \
    SEA_SET_FATPTR_SLOT0(intmd0, nd_slot0);                                    \
    SEA_SET_FATPTR_SLOT1(intmd0, nd_slot1);                                    \
    (SRC) = (typeof(SRC))intmd0;                                               \
  } while (0)

// NOTE: intrinsic
#define SEA_MKSHR(SRC)                                                         \
  do {                                                                         \
    (SRC) = (typeof(SRC))sea_mkshr((char *)(SRC));                             \
  } while (0)

// NOTE: intrinsic
#define SEA_BEGIN_UNIQUE(SRC)                                                  \
  do {                                                                         \
    (SRC) = (typeof(SRC))sea_begin_unique((char *)(SRC));                      \
  } while (0)

// NOTE: intrinsic
#define SEA_END_UNIQUE(SRC)                                                    \
  do {                                                                         \
    (SRC) = (typeof(SRC))sea_end_unique((char *)(SRC));                        \
  } while (0)

// NOTE: intrinsic
#define SEA_BEGIN_UNIQUE_AND_LOAD_CACHE(SRC, VAL)                              \
  do {                                                                         \
    (SRC) = (typeof(SRC))sea_begin_unique((char *)SRC);                        \
    SEA_SET_FATPTR_SLOT0((SRC), VAL);                                          \
  } while (0)

// TODO: make cache nd after unloading
// NOTE: intrinsic
#define SEA_UNLOAD_CACHE_AND_END_UNIQUE(SRC, DSTADDRESS, DSTLEN)               \
  do {                                                                         \
    char uniqval = __sea_get_extptr_slot0_hm((char *)SRC);                     \
    (SRC) = (typeof(SRC))sea_end_unique((char *)(SRC));                        \
    memset((char *)DSTADDRESS, uniqval, 1 /* FIXME: use DSTLEN */);            \
  } while (0)

// NOTE: intrinsic
#define SEA_WRITE_CACHE(SRC, VAL)                                              \
  do {                                                                         \
    SEA_SET_FATPTR_SLOT0((SRC), (VAL));                                        \
  } while (0)

// NOTE: intrinsic
#define SEA_READ_CACHE(VAL, SRC)                                               \
  do {                                                                         \
    SEA_GET_FATPTR_SLOT0((char *)SRC, (VAL));                                  \
  } while (0)

// NOTE: intrinsic
#define SEA_BORROW(BOR, SRC)                                                   \
  do {                                                                         \
    (BOR) = (typeof(BOR))sea_bor_mkbor((char *)SRC);                           \
    (SRC) = (typeof(SRC))sea_bor_mksuc((char *)SRC);                           \
    uint64_t brval;                                                            \
    SEA_GET_FATPTR_SLOT0((SRC), brval);                                        \
    uint64_t retval;                                                           \
    SEA_GET_FATPTR_SLOT1((SRC), retval);                                       \
    SEA_SET_FATPTR_SLOT0((BOR), brval);                                        \
    uint64_t ndval = nd_uint64t();                                             \
    SEA_SET_FATPTR_SLOT1((BOR), ndval)                                         \
    SEA_SET_FATPTR_SLOT0((SRC), ndval);                                        \
  } while (0)

// NOTE: intrinsic
#define SEA_DIE(SRC)                                                           \
  do {                                                                         \
    uint64_t nd_retval;                                                        \
    SEA_GET_FATPTR_SLOT1((char *)(SRC), nd_retval);                            \
    uint64_t cacheval;                                                         \
    SEA_GET_FATPTR_SLOT0((char *)(SRC), cacheval);                             \
    assume(nd_retval == cacheval);                                             \
    sea_die((char *)(SRC));                                                    \
  } while (0)

// NOTE: intrinsic
#define SEA_LOAD_CACHE_AND_BORROW(BOR, SRC, VAL)                               \
  do {                                                                         \
    char *intmd = __sea_set_extptr_slot0_hm((char *)(SRC), (VAL));             \
    SEA_BORROW((BOR), intmd);                                                  \
  } while (0)

// NOTE: intrinsic
#define SEA_BORROW_OFFSET(BOR_OFF, SRC, OFFSET)                                \
  do {                                                                         \
    char *boroff_intmd0;                                                       \
    SEA_BORROW(boroff_intmd0, SRC);                                            \
    (BOR_OFF) = ((typeof(BOR_OFF))(boroff_intmd0)) + OFFSET;                   \
  } while (0)

// NOTE: intrinsic
#define SEA_BORROW_LOAD(BOR, PTR_TO_SRC_PTR)                                   \
  do {                                                                         \
    char *intmd_ptrptrto = sea_bor_mem2reg((char *)(PTR_TO_SRC_PTR));          \
    typeof(BOR)(mem2reg_ptr) = *((typeof(PTR_TO_SRC_PTR))intmd_ptrptrto);      \
    SEA_BORROW(BOR, mem2reg_ptr);                                              \
    *(PTR_TO_SRC_PTR) = mem2reg_ptr;                                           \
  } while (0)

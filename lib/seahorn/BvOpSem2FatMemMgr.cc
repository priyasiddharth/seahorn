#include "BvOpSem2Context.hh"
#include "BvOpSem2ExtraWideMemMgr.hh"
#include "BvOpSem2MemManagerMixin.hh"
#include "BvOpSem2RawMemMgr.hh"

#include "llvm/IR/GetElementPtrTypeIterator.h"
#include "llvm/Support/Format.h"

#include "seahorn/Expr/ExprLlvm.hh"
#include "seahorn/Expr/ExprOpStruct.hh"
#include "seahorn/Support/SeaDebug.h"
#include "seahorn/Support/SeaLog.hh"

namespace seahorn {
namespace details {

static const unsigned int g_slotBitWidth = 64;
static const unsigned int g_slotByteWidth = g_slotBitWidth / 8;
static const unsigned int g_undefSlot0 = 0xDEF0;
static const unsigned int g_undefSlot1 = 0xDEF1;

static const unsigned int g_maxFatSlots = 2;
/// \brief provides Fat pointers and Fat memory to store them
template <class T> class FatMemManager : public MemManagerCore {
public:
  /// Right now everything is an expression. In the future, we might have
  /// other types for PtrTy, such as a tuple of expressions
  using MainPtrTy = typename T::PtrTy;
  using RawPtrTy = OpSemMemManager::PtrTy;
  using MainMemValTy = typename T::MemValTy;
  using RawMemValTy = OpSemMemManager::MemValTy;
  /// PtrTy representation for this manager
  ///
  /// Currently internal representation is just an Expr
  struct PtrTyImpl {
    Expr m_v;

    PtrTyImpl(MainPtrTy &&main, Expr &&slot0, Expr &&slot1) {
      m_v = strct::mk(std::move(main), std::move(slot0), std::move(slot1));
    }

    PtrTyImpl(const MainPtrTy &main, const Expr slot0, const Expr &slot1) {
      m_v = strct::mk(main, slot0, slot1);
    }

    explicit PtrTyImpl(const Expr &e) {
      // Our base is a struct of three exprs
      assert(strct::isStructVal(e));
      m_v = e;
    }

    Expr v() const { return m_v; }
    Expr toExpr() const { return v(); }
    explicit operator Expr() const { return toExpr(); }

    MainPtrTy getMain() { return strct::extractVal(m_v, 0); }

    MainPtrTy getRaw() { return getMain(); }

    Expr getSlot0() { return strct::extractVal(m_v, 1); }

    Expr getSlot1() { return strct::extractVal(m_v, 2); }

    Expr getSlot(unsigned idx) {
      assert(idx < g_maxFatSlots + 1);
      return strct::extractVal(m_v, idx);
    }
  };

  struct MemValTyImpl {
    Expr m_v;

    MemValTyImpl(MainMemValTy &&main_val, Expr &&slot0_val, Expr &&slot1_val) {
      assert(!strct::isStructVal(slot0_val));
      assert(!strct::isStructVal(slot1_val));
      m_v = strct::mk(std::move(main_val), std::move(slot0_val),
                      std::move(slot1_val));
    }

    MemValTyImpl(const MainMemValTy &main_val, const Expr &slot0_val,
                 const Expr &slot1_val) {
      assert(!strct::isStructVal(slot0_val));
      assert(!strct::isStructVal(slot1_val));
      m_v = strct::mk(main_val, slot0_val, slot1_val);
    }

    explicit MemValTyImpl(const Expr &e) {
      // Our base is Expr() or a struct of three exprs
      assert(!e || strct::isStructVal(e));
      assert(!e || !strct::isStructVal(e->arg(1)));
      assert(!e || !strct::isStructVal(e->arg(2)));
      m_v = e;
    }

    Expr v() const { return m_v; }
    Expr toExpr() const { return v(); }
    explicit operator Expr() const { return toExpr(); }

    MainMemValTy getMain() { return !m_v ? m_v : strct::extractVal(m_v, 0); }

    MainMemValTy getRaw() { return getMain(); }

    Expr getSlot0() { return !m_v ? m_v : strct::extractVal(m_v, 1); }

    Expr getSlot1() { return !m_v ? m_v : strct::extractVal(m_v, 2); }
  };

  using FatMemTag = MemoryFeatures::FatMem_tag;
  using TrackingTag = typename T::TrackingTag;
  using WideMemTag = typename T::WideMemTag;

  using MemValTy = MemValTyImpl;
  using PtrTy = PtrTyImpl;

  using MainPtrSortTy = typename T::PtrSortTy;
  using MainMemSortTy = typename T::MemSortTy;
  using MemRegTy = OpSemMemManager::MemRegTy;

  // TODO: change all slot0,1 methods to return these types for easier
  // reading
  using Slot0ValTy = Expr;
  using Slot1ValTy = Expr;

  struct PtrSortTyImpl {
    Expr m_ptr_sort;

    PtrSortTyImpl(MainPtrSortTy &&ptr_sort, Expr &&slot0_sort,
                  Expr &&slot1_sort) {
      m_ptr_sort = sort::structTy(std::move(ptr_sort), std::move(slot0_sort),
                                  std::move(slot1_sort));
    }

    PtrSortTyImpl(const MainPtrSortTy &ptr_sort, const Expr &slot0_sort,
                  const Expr &slot1_sort) {
      m_ptr_sort = sort::structTy(ptr_sort, slot0_sort, slot1_sort);
    }

    Expr v() const { return m_ptr_sort; }
    Expr toExpr() const { return v(); }
    explicit operator Expr() const { return toExpr(); }

    MainPtrSortTy getBaseSort() { return m_ptr_sort->arg(0); }
  };

  struct MemSortTyImpl {
    Expr m_mem_sort;

    MemSortTyImpl(MainMemSortTy &&mem_sort, Expr &&slot0_sort,
                  Expr &&slot1_sort) {
      m_mem_sort = sort::structTy(std::move(mem_sort), std::move(slot0_sort),
                                  std::move(slot1_sort));
    }

    MemSortTyImpl(const MainMemSortTy &mem_sort, Expr &slot0_sort,
                  const Expr &slot1_sort) {
      m_mem_sort = sort::structTy(mem_sort, slot0_sort, slot1_sort);
    }

    Expr v() const { return m_mem_sort; }
    Expr toExpr() const { return v(); }
    explicit operator Expr() const { return toExpr(); }
  };

  using PtrSortTy = PtrSortTyImpl;
  using MemSortTy = MemSortTyImpl;

private:
  /// \brief Memory manager for raw pointers
  T m_main;
  RawMemManager m_slot0;
  RawMemManager m_slot1;

  /// \brief A null pointer expression (cache)
  PtrTy m_nullPtr;

  /// \brief Converts a raw ptr to fat ptr with default value for fat
  PtrTy mkFatPtr(MainPtrTy mainPtr) const {
    return PtrTy(mainPtr, m_ctx.alu().ui(g_undefSlot0, g_slotBitWidth),
                 m_ctx.alu().ui(g_undefSlot1, g_slotBitWidth));
  }

  /// \brief Converts a raw ptr to fat ptr with default value for fat
  PtrTy mkFatPtr(MainPtrTy mainPtr, Slot0ValTy data0, Slot1ValTy data1) const {
    // TODO: check if data0 and data1 bitwidth is <= max or always
    // guaranteed?
    return PtrTy(mainPtr, data0, data1);
  }

  /// \brief Update a given fat pointer with a "main" address value
  PtrTy mkFatPtr(MainPtrTy mainPtr, PtrTy fat) const {
    if (fat.v()->arity() == 1)
      return mkFatPtr(mainPtr);

    llvm::SmallVector<Expr, g_maxFatSlots + 1> kids;
    assert(fat.v()->arity() == g_maxFatSlots + 1);
    kids.push_back(mainPtr);
    for (unsigned i = 1, sz = g_maxFatSlots; i < sz; ++i) {
      kids.push_back(fat.v()->arg(i));
    }
    return PtrTy(strct::mk(kids));
  }

  /// \brief Extracts a "main" pointer out of a fat pointer
  MainPtrTy mkMainPtr(PtrTy fatPtr) const {
    assert(strct::isStructVal(fatPtr.v()));
    return fatPtr.getMain();
  }

  /// \brief Extracts a "main" memory value from a fat memory value
  MainMemValTy mkMainMem(MemValTy fatMem) const {
    assert(strct::isStructVal(fatMem.v()));
    return fatMem.getMain();
  }

  RawMemValTy mkSlot0Mem(MemValTy fatMem) const {
    assert(strct::isStructVal(fatMem.v()));
    return fatMem.getSlot0();
  }

  RawMemValTy mkSlot1Mem(MemValTy fatMem) const {
    assert(strct::isStructVal(fatMem.v()));
    return fatMem.getSlot1();
  }

  /// \brief Creates a fat memory value from raw memory with given values
  /// for fat
  MemValTy mkFatMem(MainMemValTy mainMem, MainMemValTy slot0Mem,
                    MainMemValTy slot1Mem) const {
    return MemValTy(mainMem, slot0Mem, slot1Mem);
  }

public:
  FatMemManager(Bv2OpSem &sem, Bv2OpSemContext &ctx, unsigned ptrSz,
                unsigned wordSz, bool useLambdas = false);

  ~FatMemManager() = default;

  PtrSortTy ptrSort() const {
    return PtrSortTy(m_main.ptrSort(), m_ctx.alu().intTy(g_slotBitWidth),
                     m_ctx.alu().intTy(g_slotBitWidth));
  }

  /// \brief Allocates memory on the stack and returns a pointer to it
  /// \param align is requested alignment. If 0, default alignment is used
  PtrTy salloc(unsigned bytes, uint32_t align = 0) {
    auto e = m_main.salloc(bytes, align);
    // TODO: remove commented lines
    // m_slot0.salloc(bytes, align);
    // m_slot1.salloc(bytes, align);
    return mkFatPtr(e);
  }

  /// \brief Allocates memory on the stack and returns a pointer to it
  PtrTy salloc(Expr elmts, unsigned typeSz, uint32_t align = 0) {
    auto e = m_main.salloc(elmts, typeSz, align);
    // TODO: remove commented lines
    // m_slot0.salloc(elmts, typeSz, align);
    // m_slot1.salloc(elmts, typeSz, align);
    return mkFatPtr(e);
  }

  /// \brief Returns a pointer value for a given stack allocation
  PtrTy mkStackPtr(unsigned offset) {
    return mkFatPtr(m_main.mkStackPtr(offset));
  }

  /// \brief Pointer to start of the heap
  PtrTy brk0Ptr() { return mkFatPtr(m_main.brk0Ptr()); }

  /// \brief Allocates memory on the heap and returns a pointer to it
  PtrTy halloc(unsigned _bytes, uint32_t align = 0) {
    return mkFatPtr(m_main.halloc(_bytes, align));
  }

  /// \brief Allocates memory on the heap and returns pointer to it
  PtrTy halloc(Expr bytes, uint32_t align = 0) {
    return mkFatPtr(m_main.halloc(bytes, align));
  }

  /// \brief Allocates memory in global (data/bss) segment for given global
  PtrTy galloc(const GlobalVariable &gv, uint32_t align = 0) {
    m_slot0.galloc(gv, align);
    m_slot1.galloc(gv, align);
    return mkFatPtr(m_main.galloc(gv, align));
  }

  /// \brief Allocates memory in code segment for the code of a given
  /// function
  PtrTy falloc(const Function &fn) { return mkFatPtr(m_main.falloc(fn)); }

  /// \brief Returns a function pointer value for a given function
  PtrTy getPtrToFunction(const Function &F) {
    return mkFatPtr(m_main.getPtrToFunction(F));
  }

  /// \brief Returns a pointer to a global variable
  PtrTy getPtrToGlobalVariable(const GlobalVariable &gv) {
    return mkFatPtr(m_main.getPtrToGlobalVariable(gv));
  }

  /// \brief Initialize memory used by the global variable
  void initGlobalVariable(const GlobalVariable &gv) const {
    m_main.initGlobalVariable(gv);
  }

  /// \brief Creates a non-deterministic pointer that is aligned
  ///
  /// Top bits of the pointer are named by \p name and last \c log2(align)
  /// bits are set to zero
  PtrTy mkAlignedPtr(Expr name, uint32_t align) const {
    return mkFatPtr(m_main.mkAlignedPtr(name, align));
  }

  /// \brief Returns sort of a pointer register for an instruction
  PtrSortTy mkPtrRegisterSort(const Instruction &inst) const {
    return PtrSortTy(m_main.mkPtrRegisterSort(inst),
                     m_ctx.alu().intTy(g_slotBitWidth),
                     m_ctx.alu().intTy(g_slotBitWidth));
  }

  /// \brief Returns sort of a pointer register for a function pointer
  PtrSortTy mkPtrRegisterSort(const Function &fn) const { return ptrSort(); }

  /// \brief Returns sort of a pointer register for a global pointer
  PtrSortTy mkPtrRegisterSort(const GlobalVariable &gv) const {
    return ptrSort();
  }

  /// \brief Returns sort of memory-holding register for an instruction
  MemSortTy mkMemRegisterSort(const Instruction &inst) const {
    return MemSortTy(m_main.mkMemRegisterSort(inst),
                     m_slot0.mkMemRegisterSort(inst),
                     m_slot1.mkMemRegisterSort(inst));
  }

  /// \brief Returns a fresh aligned pointer value
  PtrTy freshPtr() { return mkFatPtr(m_main.freshPtr()); }

  /// \brief Returns a null ptr
  PtrTy nullPtr() const { return m_nullPtr; }

  /// \brief Fixes the type of a havoced value to mach the representation
  /// used by mem repr.
  ///
  /// \param sort
  /// \param val
  /// \return the coerced value.
  Expr coerce(Expr sort, Expr val) {
    if (strct::isStructVal(val)) {
      // recursively coerce struct-ty
      llvm::SmallVector<Expr, 8> kids;
      assert(isOp<STRUCT_TY>(sort));
      assert(sort->arity() == val->arity());
      for (unsigned i = 0, sz = val->arity(); i < sz; ++i)
        kids.push_back(coerce(sort->arg(i), val->arg(i)));
      return strct::mk(kids);
    }

    return m_main.coerce(sort, val);
  }

  /// \brief Loads an integer of a given size from 'raw' memory register
  ///
  /// \param[in] ptr pointer being accessed
  /// \param[in] mem memory value into which \p ptr points
  /// \param[in] byteSz size of the integer in bytes
  /// \param[in] align known alignment of \p ptr
  /// \return symbolic value of the read integer
  Expr loadIntFromMem(PtrTy ptr, MemValTy mem, unsigned byteSz,
                      uint64_t align) {
    return m_main.loadIntFromMem(mkMainPtr(ptr), mkMainMem(mem), byteSz, align);
  }

  /// \brief Loads a pointer stored in memory
  /// \sa loadIntFromMem
  PtrTy loadPtrFromMem(PtrTy ptr, MemValTy mem, unsigned byteSz,
                       uint64_t align) {
    MainMemValTy rawVal =
        m_main.loadPtrFromMem(mkMainPtr(ptr), mkMainMem(mem), byteSz, align);
    MainMemValTy slot0Val = m_slot0.loadIntFromMem(
        mkMainPtr(ptr), mkSlot0Mem(mem), g_slotByteWidth, align);
    MainMemValTy slot1Val = m_slot1.loadIntFromMem(
        mkMainPtr(ptr), mkSlot1Mem(mem), g_slotByteWidth, align);
    return mkFatPtr(rawVal, slot0Val, slot1Val);
  }

  /// \brief Pointer addition with numeric offset
  PtrTy ptrAdd(PtrTy ptr, int32_t _offset) const {
    MainPtrTy rawPtr = m_main.ptrAdd(mkMainPtr(ptr), _offset);
    return mkFatPtr(rawPtr, ptr);
  }

  /// \brief Pointer addition with symbolic offset
  PtrTy ptrAdd(PtrTy ptr, Expr offset) const {
    MainPtrTy rawPtr = m_main.ptrAdd(mkMainPtr(ptr), offset);
    return mkFatPtr(rawPtr, ptr);
  }

  /// \brief Stores an integer into memory
  ///
  /// Returns an expression describing the state of memory in \c memReadReg
  /// after the store
  /// \sa loadIntFromMem
  MemValTy storeIntToMem(Expr _val, PtrTy ptr, MemValTy mem, unsigned byteSz,
                         uint64_t align) {
    return mkFatMem(m_main.storeIntToMem(_val, mkMainPtr(ptr), mkMainMem(mem),
                                         byteSz, align),
                    mkSlot0Mem(mem), mkSlot1Mem(mem));
  }

  /// \brief Stores a pointer into memory
  /// \sa storeIntToMem
  MemValTy storePtrToMem(PtrTy val, PtrTy ptr, MemValTy mem, unsigned byteSz,
                         uint64_t align) {
    MainMemValTy main = m_main.storePtrToMem(mkMainPtr(val), mkMainPtr(ptr),
                                             mkMainMem(mem), byteSz, align);
    MainMemValTy slot0 =
        m_slot0.storeIntToMem(getFatData(val, 0), mkMainPtr(ptr),
                              mkSlot0Mem(mem), g_slotByteWidth, align);
    MainMemValTy slot1 =
        m_slot1.storeIntToMem(getFatData(val, 1), mkMainPtr(ptr),
                              mkSlot1Mem(mem), g_slotByteWidth, align);
    auto res = mkFatMem(main, slot0, slot1);
    return res;
  }

  /// \brief Returns an expression corresponding to a load from memory
  ///
  /// \param[in] ptr is the pointer being dereferenced
  /// \param[in] memReg is the memory register being read
  /// \param[in] ty is the type of value being loaded
  /// \param[in] align is the known alignment of the load
  Expr loadValueFromMem(PtrTy ptr, MemValTy mem, const llvm::Type &ty,
                        uint64_t align) {

    const unsigned byteSz =
        m_sem.getTD().getTypeStoreSize(const_cast<llvm::Type *>(&ty));
    // ExprFactory &efac = ptr.v()->efac();

    Expr res;
    switch (ty.getTypeID()) {
    case Type::IntegerTyID:
      res = loadIntFromMem(ptr, mem, byteSz, align);
      if (res && ty.getScalarSizeInBits() < byteSz * 8)
        res = m_ctx.alu().doTrunc(res, ty.getScalarSizeInBits());
      break;
    case Type::FloatTyID:
    case Type::DoubleTyID:
    case Type::X86_FP80TyID:
      errs() << "Error: load of float/double is not supported\n";
      llvm_unreachable(nullptr);
      break;
    case Type::FixedVectorTyID:
    case Type::ScalableVectorTyID:
      errs() << "Error: load of fixed vectors is not supported\n";
      llvm_unreachable(nullptr);
      break;
    case Type::PointerTyID:
      res = loadPtrFromMem(ptr, mem, byteSz, align).v();
      break;
    case Type::StructTyID:
      errs() << "loading form struct type " << ty << " is not supported";
      return res;
    default:
      SmallString<256> msg;
      raw_svector_ostream out(msg);
      out << "Loading from type: " << ty << " is not supported\n";
      assert(false);
    }
    return res;
  }

  MemValTy storeValueToMem(Expr _val, PtrTy ptr, MemValTy memIn,
                           const llvm::Type &ty, uint32_t align) {
    assert(ptr.v());
    Expr val = _val;
    const unsigned byteSz =
        m_sem.getTD().getTypeStoreSize(const_cast<llvm::Type *>(&ty));
    // ExprFactory &efac = ptr.v()->efac();

    MemValTy res = MemValTy(Expr());
    switch (ty.getTypeID()) {
    case Type::IntegerTyID:
      if (ty.getScalarSizeInBits() < byteSz * 8) {
        val = m_ctx.alu().doZext(val, byteSz * 8, ty.getScalarSizeInBits());
      }
      res = storeIntToMem(val, ptr, memIn, byteSz, align);
      break;
    case Type::FloatTyID:
    case Type::DoubleTyID:
    case Type::X86_FP80TyID:
      errs() << "Error: store of float/double is not supported\n";
      llvm_unreachable(nullptr);
      break;
    case Type::FixedVectorTyID:
    case Type::ScalableVectorTyID:
      errs() << "Error: store of vectors is not supported\n";
      llvm_unreachable(nullptr);
      break;
    case Type::PointerTyID:
      res = storePtrToMem(PtrTy(val), ptr, memIn, byteSz, align);
      break;
    case Type::StructTyID:
      WARN << "Storing struct type " << ty << " is not supported\n";
      return res;
    default:
      SmallString<256> msg;
      raw_svector_ostream out(msg);
      out << "Loading from type: " << ty << " is not supported\n";
      assert(false);
      report_fatal_error(out.str());
    }
    return res;
  }

  /// \brief Executes symbolic memset with a concrete length
  MemValTy MemSet(PtrTy ptr, Expr _val, unsigned len, MemValTy mem,
                  uint32_t align) {
    return mkFatMem(
        m_main.MemSet(mkMainPtr(ptr), _val, len, mkMainMem(mem), align),
        mkSlot0Mem(mem), mkSlot1Mem(mem));
  }

  MemValTy MemSet(PtrTy ptr, Expr _val, Expr len, MemValTy mem,
                  uint32_t align) {
    return mkFatMem(
        m_main.MemSet(mkMainPtr(ptr), _val, len, mkMainMem(mem), align),
        mkSlot0Mem(mem), mkSlot1Mem(mem));
  }

  /// \brief Executes symbolic memcpy with concrete length
  MemValTy MemCpy(PtrTy dPtr, PtrTy sPtr, unsigned len, MemValTy memTrsfrRead,
                  MemValTy memRead, uint32_t align) {
    return mkFatMem(
        m_main.MemCpy(mkMainPtr(dPtr), mkMainPtr(sPtr), len,
                      mkMainMem(memTrsfrRead), mkMainMem(memRead), align),
        m_slot0.MemCpy(mkMainPtr(dPtr), mkMainPtr(sPtr), len,
                       mkSlot0Mem(memTrsfrRead), mkSlot0Mem(memRead), align),
        m_slot1.MemCpy(mkMainPtr(dPtr), mkMainPtr(sPtr), len,
                       mkSlot1Mem(memTrsfrRead), mkSlot1Mem(memRead), align));
  }

  /// \brief Executes symbolic memcpy with concrete length
  MemValTy MemCpy(PtrTy dPtr, PtrTy sPtr, Expr len, MemValTy memTrsfrRead,
                  MemValTy memRead, uint32_t align) {
    return mkFatMem(
        m_main.MemCpy(mkMainPtr(dPtr), mkMainPtr(sPtr), len,
                      mkMainMem(memTrsfrRead), mkMainMem(memRead), align),
        m_slot0.MemCpy(mkMainPtr(dPtr), mkMainPtr(sPtr), len,
                       mkSlot0Mem(memTrsfrRead), mkSlot0Mem(memRead), align),
        m_slot1.MemCpy(mkMainPtr(dPtr), mkMainPtr(sPtr), len,
                       mkSlot1Mem(memTrsfrRead), mkSlot1Mem(memRead), align));
  }

  /// \brief Executes symbolic memcpy from physical memory with concrete
  /// length
  MemValTy MemFill(PtrTy dPtr, char *sPtr, unsigned len, MemValTy mem,
                   uint32_t align = 0) {
    return mkFatMem(
        m_main.MemFill(mkMainPtr(dPtr), sPtr, len, mkMainMem(mem), align),
        mkSlot0Mem(mem), mkSlot1Mem(mem));
  }

  /// \brief Executes inttoptr conversion
  PtrTy inttoptr(Expr intVal, const Type &intTy, const Type &ptrTy) const {
    return mkFatPtr(m_main.inttoptr(intVal, intTy, ptrTy));
  }

  /// \brief Executes ptrtoint conversion. This only converts the raw ptr to
  /// int.
  Expr ptrtoint(PtrTy ptr, const Type &ptrTy, const Type &intTy) const {
    return m_main.ptrtoint(mkMainPtr(ptr), ptrTy, intTy);
  }

  Expr ptrUlt(PtrTy p1, PtrTy p2) const {
    return m_main.ptrUlt(mkMainPtr(p1), mkMainPtr(p2));
  }
  Expr ptrSlt(PtrTy p1, PtrTy p2) const {
    return m_main.ptrSlt(mkMainPtr(p1), mkMainPtr(p2));
  }
  Expr ptrUle(PtrTy p1, PtrTy p2) const {
    return m_main.ptrUle(mkMainPtr(p1), mkMainPtr(p2));
  }
  Expr ptrSle(PtrTy p1, PtrTy p2) const {
    return m_main.ptrSle(mkMainPtr(p1), mkMainPtr(p2));
  }
  Expr ptrUgt(PtrTy p1, PtrTy p2) const {
    return m_main.ptrUgt(mkMainPtr(p1), mkMainPtr(p2));
  }
  Expr ptrSgt(PtrTy p1, PtrTy p2) const {
    return m_main.ptrSgt(mkMainPtr(p1), mkMainPtr(p2));
  }
  Expr ptrUge(PtrTy p1, PtrTy p2) const {
    return m_main.ptrUge(mkMainPtr(p1), mkMainPtr(p2));
  }
  Expr ptrSge(PtrTy p1, PtrTy p2) const {
    return m_main.ptrSge(mkMainPtr(p1), mkMainPtr(p2));
  }

  /// \brief Checks if two pointers are equal.
  Expr ptrEq(PtrTy p1, PtrTy p2) const {
    return m_main.ptrEq(mkMainPtr(p1), mkMainPtr(p2));
  }
  Expr ptrNe(PtrTy p1, PtrTy p2) const {
    return m_main.ptrNe(mkMainPtr(p1), mkMainPtr(p2));
  }

  Expr ptrSub(PtrTy p1, PtrTy p2) const {
    return m_main.ptrSub(mkMainPtr(p1), mkMainPtr(p2));
  }

  /// \brief Computes a pointer corresponding to the gep instruction
  PtrTy gep(PtrTy ptr, gep_type_iterator it, gep_type_iterator end) const {
    // Here the resultant pointer automatically gets the same slot(s) data
    // as the original. Therefore we don't require the client to manually
    // update slot(s) data after a gep call.
    MainPtrTy rawPtr = m_main.gep(mkMainPtr(ptr), it, end);
    return mkFatPtr(rawPtr, ptr);
  }

  /// \brief Called when a function is entered for the first time
  void onFunctionEntry(const Function &fn) {
    m_main.onFunctionEntry(fn);
    m_slot0.onFunctionEntry(fn);
    m_slot1.onFunctionEntry(fn);
  }

  /// \brief Called when a module entered for the first time
  void onModuleEntry(const Module &M) {
    m_main.onModuleEntry(M);
    m_slot0.onModuleEntry(M);
    m_slot1.onModuleEntry(M);
  }

  /// \brief Debug helper
  void dumpGlobalsMap() { m_main.dumpGlobalsMap(); }

  std::pair<char *, unsigned>
  getGlobalVariableInitValue(const llvm::GlobalVariable &gv) {
    // TODO: do we need to make a union
    return m_main.getGlobalVariableInitValue(gv);
  }

  MemValTy zeroedMemory() const {
    return mkFatMem(m_main.zeroedMemory(), m_slot0.zeroedMemory(),
                    m_slot1.zeroedMemory());
  }

  Expr getFatData(PtrTy p, unsigned SlotIdx) { return p.getSlot(1 + SlotIdx); }

  PtrTy setFatData(PtrTy p, unsigned slotIdx, Expr data) {
    assert(slotIdx < g_maxFatSlots);
    // TODO: generalize to more slots
    if (slotIdx == 0) {
      return PtrTy(p.getMain(), data, p.getSlot1());
    } else if (slotIdx == 1) {
      return PtrTy(p.getMain(), p.getSlot0(), data);
    } else {
      assert(0); // should not be hit
      return p;
    }
  }

  RawPtrTy getAddressable(PtrTy p) const {
    return m_main.getAddressable(p.getMain());
  }

  bool isPtrTyVal(Expr e) const {
    // struct with raw ptr + fat slots
    return e && strct::isStructVal(e) && e->arity() == (1 + g_maxFatSlots);
  }

  bool isMemVal(Expr e) const {
    // struct with raw ptr + fat slots
    return e && strct::isStructVal(e) && e->arity() == (1 + g_maxFatSlots);
  }

  Expr isMetadataSet(MetadataKind kind, PtrTy ptr, MemValTy mem) {
    // The width of the value will be wordSz
    Expr val = getMetadata(kind, ptr, mem, 1);
    if (val == Expr()) {
      return m_ctx.alu().getTrue();
    }
    auto sentinel = m_ctx.alu().ui(1, getMetadataMemWordSzInBits());
    return m_ctx.alu().doEq(val, sentinel, getMetadataMemWordSzInBits());
  }

  MemValTy memsetMetadata(MetadataKind kind, PtrTy ptr, unsigned int len,
                          MemValTy memIn, unsigned int val) {
    auto mainOut =
        m_main.memsetMetadata(kind, ptr.getMain(), len, memIn.getMain(), val);
    return MemValTy(mainOut, memIn.getSlot0(), memIn.getSlot1());
  }

  MemValTy memsetMetadata(MetadataKind kind, PtrTy ptr, Expr len,
                          MemValTy memIn, unsigned int val) {
    auto mainOut =
        m_main.memsetMetadata(kind, ptr.getMain(), len, memIn.getMain(), val);
    return MemValTy(mainOut, memIn.getSlot0(), memIn.getSlot1());
  }

  Expr getMetadata(MetadataKind kind, PtrTy ptr, MemValTy memIn,
                   unsigned int byteSz) {
    return m_main.getMetadata(kind, ptr.getMain(), memIn.getMain(), byteSz);
  }

  unsigned int getMetadataMemWordSzInBits() {
    return m_main.getMetadataMemWordSzInBits();
  }

  size_t getNumOfMetadataSlots() { return m_main.getNumOfMetadataSlots(); }
  MemValTy setMetadata(MetadataKind kind, PtrTy ptr, MemValTy mem, Expr val) {
    if (!m_ctx.isTrackingOn() && kind != MetadataKind::ALLOC) {
      LOG("opsem.memtrack.verbose",
          WARN << "Ignoring setMetadata();Memory tracking is off"
               << "\n";);
      return mem;
    }
    auto mainOut = m_main.setMetadata(kind, ptr.getMain(), mem.getMain(), val);
    return MemValTy(mainOut, mem.getSlot0(), mem.getSlot1());
  }

  Expr isDereferenceable(PtrTy p, Expr byteSz) {
    return m_main.isDereferenceable(p.getMain(), byteSz);
  }
};

template <class T>
FatMemManager<T>::FatMemManager(Bv2OpSem &sem, Bv2OpSemContext &ctx,
                                unsigned ptrSz, unsigned wordSz,
                                bool useLambdas)
    : MemManagerCore(sem, ctx, ptrSz, wordSz,
                     false /* this is a nop since we delegate to T MemMgr */),
      m_main(sem, ctx, ptrSz, wordSz, useLambdas),
      m_slot0(sem, ctx, ptrSz, g_slotByteWidth, useLambdas),
      m_slot1(sem, ctx, ptrSz, g_slotByteWidth, useLambdas),
      m_nullPtr(mkFatPtr(m_main.nullPtr())) {}

OpSemMemManager *mkFatMemManager(Bv2OpSem &sem, Bv2OpSemContext &ctx,
                                 unsigned ptrSz, unsigned wordSz,
                                 bool useLambdas) {
  return new OpSemMemManagerMixin<FatMemManager<RawMemManager>>(
      sem, ctx, ptrSz, wordSz, useLambdas);
}

// FatMemManager with ExtraWide and Tracking components
OpSemMemManager *mkFatMemEWWTManager(Bv2OpSem &sem, Bv2OpSemContext &ctx,
                                     unsigned ptrSz, unsigned wordSz,
                                     bool useLambdas) {
  return new OpSemMemManagerMixin<FatMemManager<EWWTMemManager>>(
      sem, ctx, ptrSz, wordSz, useLambdas);
}

} // namespace details
} // namespace seahorn

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

#include <boost/variant.hpp>

static llvm::cl::opt<unsigned>
    FatMemSlots("horn-bv2-num-fat-slots",
                llvm::cl::desc("Number of fat slots to initiate FatMemManager with"),
                llvm::cl::init(3));

namespace seahorn {
namespace details {

static const unsigned int g_slotBitWidth = 64;
static const unsigned int g_slotByteWidth = g_slotBitWidth / 8;

// TODO: remove these vars
// static const unsigned int g_undefSlot0 = 0xDEF0;
// static const unsigned int g_undefSlot1 = 0xDEF1;

/// \brief provides Fat pointers and Fat memory to store them
template <class T> class FatMemManager : public MemManagerCore {
public:
  /// Right now everything is an expression. In the future, we might have
  /// other types for PtrTy, such as a tuple of expressions
  using MainPtrTy = typename T::PtrTy;
  using RawPtrTy = OpSemMemManager::PtrTy;
  using MainMemValTy = typename T::MemValTy;
  using RawMemValTy = OpSemMemManager::MemValTy;
  using AnyPtrTy = Expr;
  using AnyMemValTy = Expr;

  /// \brief Source of unique identifiers
  mutable unsigned m_id;

  llvm::Twine m_fatMemBaseName;

  /// PtrTy representation for this manager
  ///
  /// Currently internal representation is just an Expr
  struct PtrTyImpl {
    Expr m_v;

    explicit PtrTyImpl(llvm::SmallVector<AnyPtrTy, 8> &&slots) {
      assert(slots.size() == FatMemSlots + 1);
      for (auto e : slots) {
        assert(e);
      }
      m_v = strct::mk(std::move(slots));
    }

    explicit PtrTyImpl(llvm::SmallVector<AnyPtrTy, 8> &slots) {
      assert(slots.size() == FatMemSlots + 1);
      for (auto e : slots) {
        assert(e);
      }
      m_v = strct::mk(slots);
    }

    explicit PtrTyImpl(const Expr &e) {
      // Our base is a struct of three exprs
      assert(strct::isStructVal(e));
      assert(e->arity() == FatMemSlots + 1);
      for (unsigned i = 0, sz = e->arity(); i < sz; i++) {
        assert(e->arg(i));
      }
      m_v = e;
    }

    Expr v() const { return m_v; }
    Expr toExpr() const { return v(); }
    explicit operator Expr() const { return toExpr(); }

    MainPtrTy getMain() { return strct::extractVal(m_v, 0); }

    MainPtrTy getRaw() { return getMain(); }

    Expr getSlot(unsigned idx) {
      assert(idx < FatMemSlots + 1);
      auto e = strct::extractVal(m_v, idx);
      assert(e); // e is not null
      return e;
    }
  };

  struct MemValTyImpl {
    Expr m_v;

    explicit MemValTyImpl(llvm::SmallVector<AnyMemValTy, 8> &&vals) {
      //assert(!strct::isStructVal(slot0_val));
      //assert(!strct::isStructVal(slot1_val));
      assert(vals.size() == FatMemSlots + 1);
      for (auto e : vals) {
        assert(e);
      }
      // TODO: add back isStructVal check
      m_v = strct::mk(std::move(vals));
    }

    explicit MemValTyImpl(llvm::SmallVector<AnyMemValTy, 8> &vals) {
      assert(vals.size() == FatMemSlots + 1);
      //assert(!strct::isStructVal(slot0_val));
      // assert(!strct::isStructVal(slot1_val));
      for (auto e : vals) {
        assert(e);
      }

      // TODO: add back isStructVal check
      m_v = strct::mk(vals);
    }

    explicit MemValTyImpl(const Expr &e) {
      // Our base is Expr() or a struct of three exprs
      assert(!e || strct::isStructVal(e));
      for (unsigned i=0; i < FatMemSlots; i++) {
        assert(!e || !strct::isStructVal(e->arg(i + 1)));
      }
      if (e) {
        assert(e->arity() == FatMemSlots + 1);
        for (unsigned i = 0, sz = e->arity(); i < sz; i++) {
          assert(e->arg(i));
        }
      }
      m_v = e;
    }

    Expr v() const { return m_v; }
    Expr toExpr() const { return v(); }
    explicit operator Expr() const { return toExpr(); }

    MainMemValTy getMain() { return !m_v ? m_v : strct::extractVal(m_v, 0); }

    MainMemValTy getRaw() { return getMain(); }

    Expr getSlot(unsigned idx) {
      assert(idx < FatMemSlots + 1);
      auto e = strct::extractVal(m_v, idx);
      assert(e); // e is not null
      return e;
    }
  };

  using FatMemTag = MemoryFeatures::FatMem_tag;
  using TrackingTag = typename T::TrackingTag;
  using WideMemTag = typename T::WideMemTag;

  using MemValTy = MemValTyImpl;
  using PtrTy = PtrTyImpl;

  using MainPtrSortTy = typename T::PtrSortTy;
  using MainMemSortTy = typename T::MemSortTy;
  using MemRegTy = OpSemMemManager::MemRegTy;
  using AnyPtrSortTy = Expr;
  using AnyMemSortTy = Expr;

  // TODO: change all slot0,1 methods to return these types for easier
  // reading
  using Slot0ValTy = Expr;
  using Slot1ValTy = Expr;

  struct PtrSortTyImpl {
    Expr m_ptr_sort;

    explicit PtrSortTyImpl(llvm::SmallVector<AnyPtrSortTy, 8> &&ptrSorts) {
      assert(ptrSorts.size() == FatMemSlots + 1);
      for (auto e : ptrSorts) {
        assert(e);
      }
      m_ptr_sort = sort::structTy(std::move(ptrSorts));
    }

    explicit PtrSortTyImpl(llvm::SmallVector<AnyPtrSortTy, 8> &ptrSorts) {
      assert(ptrSorts.size() == FatMemSlots + 1);
      for (auto e : ptrSorts) {
        assert(e);
      }
      m_ptr_sort = sort::structTy(std::move(ptrSorts));
    }

    Expr v() const { return m_ptr_sort; }
    Expr toExpr() const { return v(); }
    explicit operator Expr() const { return toExpr(); }

    MainPtrSortTy getBaseSort() { return m_ptr_sort->arg(0); }
  };

  struct MemSortTyImpl {
    Expr m_mem_sort;

    explicit MemSortTyImpl(llvm::SmallVector<AnyMemSortTy, 8> &&memSorts) {
      assert(memSorts.size() == FatMemSlots + 1);
      for (auto e : memSorts) {
        assert(e);
      }
      m_mem_sort = sort::structTy(std::move(memSorts));
    }

    explicit MemSortTyImpl(llvm::SmallVector<AnyMemSortTy, 8> &memSorts) {
      assert(memSorts.size() == FatMemSlots + 1);
      for (auto e : memSorts) {
        assert(e);
      }
      m_mem_sort = sort::structTy(memSorts);
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
  std::vector<RawMemManager> m_slots;

  /// \brief A null pointer expression (cache)
  PtrTy m_nullPtr;

  /// \brief Converts a raw ptr to fat ptr with default value for fat
  PtrTy mkFatPtr(MainPtrTy mainPtr) const {
    llvm::SmallVector<AnyPtrTy, 8> ptrVals;
    ptrVals.push_back(mainPtr);
    // assign fresh (nd) values to fat slots
    for(unsigned i=0; i < FatMemSlots; i++) {
      llvm::SmallString<100> tempStorage;
      auto fullName = m_fatMemBaseName + "slot" + std::to_string(i);
      auto fullNameExpr = mkTerm<std::string>(fullName.toStringRef(tempStorage).str(), m_efac);
      Expr freshSlotVal = op::variant::variant(m_id, fullNameExpr);
      Expr slotBind =
        bind::mkConst(freshSlotVal, m_ctx.alu().intTy(g_slotBitWidth));
      m_id++;
      ptrVals.push_back(slotBind);
    }
    return PtrTy(ptrVals);
  }

  /// \brief Assembles a fat ptr from parts
  PtrTy mkFatPtr(llvm::SmallVector<AnyPtrTy, 8> slots) const {
    // TODO: check if data0 and data1 bitwidth is <= max or always
    // guaranteed?
    return PtrTy(slots);
  }

  /// \brief Update a given fat pointer with a "main" address value
  PtrTy updateFatPtr(MainPtrTy mainPtr, PtrTy fat) const {
    if (fat.v()->arity() == 1)
      return mkFatPtr(mainPtr);

    llvm::SmallVector<AnyPtrTy, 8> kids;
    assert(fat.v()->arity() == FatMemSlots + 1);
    kids.push_back(mainPtr);
    for (unsigned i = 1; i <= FatMemSlots; i++) {
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
  MemValTy mkFatMem(llvm::SmallVector<AnyMemValTy, 8> vals) const {
    return MemValTy(vals);
  }

public:
  FatMemManager(Bv2OpSem &sem, Bv2OpSemContext &ctx, unsigned ptrSz,
                unsigned wordSz, bool useLambdas = false);

  ~FatMemManager() = default;

  PtrSortTy ptrSort() const {
    llvm::SmallVector<AnyPtrSortTy, 8> sorts;
    sorts.push_back(m_main.ptrSort());
    for (unsigned i=0; i < FatMemSlots; i++) {
      sorts.push_back(m_ctx.alu().intTy(g_slotBitWidth));
    }
    return PtrSortTy(sorts);
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
    for (auto memMgr : m_slots) {
      memMgr.galloc(gv, align);
    }
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
    llvm::SmallVector<AnyPtrSortTy, 8> sorts;
    sorts.push_back(m_main.mkPtrRegisterSort(inst));
    for (unsigned i = 0; i < FatMemSlots; i++) {
      sorts.push_back(m_ctx.alu().intTy(g_slotBitWidth));
    }
    return PtrSortTy(sorts);
  }

  /// \brief Returns sort of a pointer register for a function pointer
  PtrSortTy mkPtrRegisterSort(const Function &fn) const { return ptrSort(); }

  /// \brief Returns sort of a pointer register for a global pointer
  PtrSortTy mkPtrRegisterSort(const GlobalVariable &gv) const {
    return ptrSort();
  }

  /// \brief Returns sort of memory-holding register for an instruction
  MemSortTy mkMemRegisterSort(const Instruction &inst) const {
    llvm::SmallVector<AnyMemSortTy, 8> sorts;
    sorts.push_back(m_main.mkMemRegisterSort(inst));
    for (auto memMgr : m_slots) {
      sorts.push_back(memMgr.mkMemRegisterSort(inst));
    }
    return MemSortTy(sorts);
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
      assert(sort->arity() == 1 + FatMemSlots);
      kids.push_back(m_main.coerce(sort->arg(0), val->arg(0)));
      for (unsigned i = 1, sz = val->arity(); i < sz; ++i)
        kids.push_back(m_slots[i - 1].coerce(sort->arg(i), val->arg(i)));
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
    llvm::SmallVector<AnyPtrTy, 8> ptrVals;

    MainPtrTy rawVal =
        m_main.loadPtrFromMem(mkMainPtr(ptr), mkMainMem(mem), byteSz, align);
    ptrVals.push_back(rawVal);
    RawPtrTy rawPtr = getAddressable(ptr);
    for(unsigned i=1; i <= FatMemSlots; i++) {
      auto slotVal = m_slots[i - 1].loadIntFromMem(rawPtr, mem.getSlot(i), g_slotByteWidth, align);
      ptrVals.push_back(slotVal);
    }
    return mkFatPtr(ptrVals);
  }

  /// \brief Pointer addition with numeric offset
  PtrTy ptrAdd(PtrTy ptr, int32_t _offset) const {
    MainPtrTy mainPtr = m_main.ptrAdd(mkMainPtr(ptr), _offset);
    return updateFatPtr(mainPtr, ptr);
  }

  /// \brief Pointer addition with symbolic offset
  PtrTy ptrAdd(PtrTy ptr, Expr offset) const {
    MainPtrTy mainPtr = m_main.ptrAdd(mkMainPtr(ptr), offset);
    return updateFatPtr(mainPtr, ptr);
  }

  /// \brief Stores an integer into memory
  ///
  /// Returns an expression describing the state of memory in \c memReadReg
  /// after the store
  /// \sa loadIntFromMem
  MemValTy storeIntToMem(Expr _val, PtrTy ptr, MemValTy mem, unsigned byteSz,
                         uint64_t align) {
    llvm::SmallVector<AnyMemValTy, 8> memVals;
    assert(!strct::isStructVal(_val));
    memVals.push_back(m_main.storeIntToMem(_val, mkMainPtr(ptr), mkMainMem(mem),
                                         byteSz, align));
    for(unsigned i=1; i <= FatMemSlots; i++) {
      memVals.push_back(mem.getSlot(i));
    }
    return mkFatMem(memVals);
  }

  /// \brief Stores a pointer into memory
  /// \sa storeIntToMem
  MemValTy storePtrToMem(PtrTy val, PtrTy ptr, MemValTy mem, unsigned byteSz,
                         uint64_t align) {
    llvm::SmallVector<AnyMemValTy, 8> memVals;

    MainMemValTy main = m_main.storePtrToMem(mkMainPtr(val), mkMainPtr(ptr),
                                             mkMainMem(mem), byteSz, align);
    memVals.push_back(main);
    RawPtrTy rawPtr = getAddressable(ptr);
    for(unsigned i=1; i <= FatMemSlots; i++) {
      MainMemValTy slotVal = m_slots[i - 1].storeIntToMem(
        val.getSlot(i), rawPtr, mem.getSlot(i), g_slotByteWidth, align);
      memVals.push_back(slotVal);
    }
    return mkFatMem(memVals);
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
    llvm::SmallVector<AnyMemValTy, 8> memVals;
    memVals.push_back(m_main.MemSet(mkMainPtr(ptr), _val, len, mkMainMem(mem), align));
    for(unsigned i=1; i <= FatMemSlots; i++) {
      memVals.push_back(mem.getSlot(i));
    }
    return mkFatMem(memVals);
  }

  MemValTy MemSet(PtrTy ptr, Expr _val, Expr len, MemValTy mem,
                  uint32_t align) {
    llvm::SmallVector<AnyMemValTy, 8> memVals;
    memVals.push_back(m_main.MemSet(mkMainPtr(ptr), _val, len, mkMainMem(mem), align));
    for(unsigned i=1; i <= FatMemSlots; i++) {
      memVals.push_back(mem.getSlot(i));
    }
    return mkFatMem(memVals);
  }

  /// \brief Executes symbolic memcpy with concrete length
  MemValTy MemCpy(PtrTy dPtr, PtrTy sPtr, unsigned len, MemValTy memTrsfrRead,
                  MemValTy memRead, uint32_t align) {
    llvm::SmallVector<AnyMemValTy, 8> memVals;
    memVals.push_back(m_main.MemCpy(mkMainPtr(dPtr), mkMainPtr(sPtr), len,
                      mkMainMem(memTrsfrRead), mkMainMem(memRead), align));
    RawPtrTy rawPtrDst = getAddressable(dPtr);
    RawPtrTy rawPtrSrc = getAddressable(sPtr);
    for(unsigned i=1; i <= FatMemSlots; i++) {
      memVals.push_back(m_slots[i - 1].MemCpy(rawPtrDst, rawPtrSrc, len, memTrsfrRead.getSlot(i),
                        memRead.getSlot(i), align));
    }
    return mkFatMem(memVals);
  }

  /// \brief Executes symbolic memcpy with concrete length
  MemValTy MemCpy(PtrTy dPtr, PtrTy sPtr, Expr len, MemValTy memTrsfrRead,
                  MemValTy memRead, uint32_t align) {
    llvm::SmallVector<AnyMemValTy, 8> memVals;
    memVals.push_back(m_main.MemCpy(mkMainPtr(dPtr), mkMainPtr(sPtr), len,
                      mkMainMem(memTrsfrRead), mkMainMem(memRead), align));
    RawPtrTy rawPtrDst = getAddressable(dPtr);
    RawPtrTy rawPtrSrc = getAddressable(sPtr);
    for(unsigned i=1; i <= FatMemSlots; i++) {
      memVals.push_back(m_slots[i - 1].MemCpy(rawPtrDst, rawPtrSrc, len, memTrsfrRead.getSlot(i),
                        memRead.getSlot(i), align));
    }
    return mkFatMem(memVals);
  }

  /// \brief Executes symbolic memcpy from physical memory with concrete
  /// length
  MemValTy MemFill(PtrTy dPtr, char *sPtr, unsigned len, MemValTy mem,
                   uint32_t align = 0) {
   llvm::SmallVector<AnyMemValTy, 8> memVals;
    memVals.push_back(m_main.MemFill(mkMainPtr(dPtr), sPtr, len, mkMainMem(mem), align));
    for(unsigned i=1; i <= FatMemSlots; i++) {
      memVals.push_back(mem.getSlot(i));
    }
    return mkFatMem(memVals);
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
    MainPtrTy mainPtr = m_main.gep(ptr.getMain(), it, end);
    return updateFatPtr(mainPtr, ptr);
  }

  /// \brief Called when a function is entered for the first time
  void onFunctionEntry(const Function &fn) {
    m_main.onFunctionEntry(fn);
    for(auto memMgr : m_slots) {
      memMgr.onFunctionEntry(fn);
    }
  }

  /// \brief Called when a module entered for the first time
  void onModuleEntry(const Module &M) {
    m_main.onModuleEntry(M);
    for(unsigned i=0; i < FatMemSlots; i++) {
      m_slots[i].onModuleEntry(M);
    }
  }

  /// \brief Debug helper
  void dumpGlobalsMap() { m_main.dumpGlobalsMap(); }

  std::pair<char *, unsigned>
  getGlobalVariableInitValue(const llvm::GlobalVariable &gv) {
    // TODO: do we need to make a union
    return m_main.getGlobalVariableInitValue(gv);
  }

  MemValTy zeroedMemory() const {
    llvm::SmallVector<AnyMemValTy, 8> memVals;
    memVals.push_back(m_main.zeroedMemory());
    for(unsigned i=1; i <= FatMemSlots; i++) {
      memVals.push_back(m_slots[i - 1].zeroedMemory());
    }
    return mkFatMem(memVals);
  }

  /// \brief get fat data in ith fat slot.
  Expr getFatData(PtrTy p, unsigned SlotIdx) { return p.getSlot(1 + SlotIdx); }

  PtrTy setFatData(PtrTy p, unsigned slotIdx, Expr data) {
    assert(slotIdx < FatMemSlots);
    llvm::SmallVector<AnyPtrTy, 8> ptrVals;
    ptrVals.push_back(p.getMain());
    for(unsigned i=1; i <= FatMemSlots; i++) {
      if (slotIdx + 1 == i) {
        ptrVals.push_back(data);
      } else {
        ptrVals.push_back(p.getSlot(i));
      }
    }
    return mkFatPtr(ptrVals);
  }

  RawPtrTy getAddressable(PtrTy p) const {
    return m_main.getAddressable(p.getMain());
  }

  bool isPtrTyVal(Expr e) const {
    // struct with raw ptr + fat slots
    return e && strct::isStructVal(e) && e->arity() == (1 + FatMemSlots);
  }

  bool isMemVal(Expr e) const {
    // struct with raw ptr + fat slots
    return e && strct::isStructVal(e) && e->arity() == (1 + FatMemSlots);
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
    llvm::SmallVector<AnyMemValTy, 8> memVals;
    memVals.push_back(m_main.memsetMetadata(kind, ptr.getMain(), len, memIn.getMain(), val));
    for(unsigned i=1; i <= FatMemSlots; i++) {
      memVals.push_back(memIn.getSlot(i));
    }
    return mkFatMem(memVals);
  }

  MemValTy memsetMetadata(MetadataKind kind, PtrTy ptr, Expr len,
                          MemValTy memIn, unsigned int val) {
    llvm::SmallVector<AnyMemValTy, 8> memVals;
    memVals.push_back(m_main.memsetMetadata(kind, ptr.getMain(), len, memIn.getMain(), val));
    for(unsigned i=1; i <= FatMemSlots; i++) {
      memVals.push_back(memIn.getSlot(i));
    }
    return memVals;
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
    llvm::SmallVector<AnyMemValTy, 8> memVals;
    auto mainOut = m_main.setMetadata(kind, ptr.getMain(), mem.getMain(), val);
    memVals.push_back(mainOut);
    for(unsigned i=1; i <= FatMemSlots; i++) {
      memVals.push_back(mem.getSlot(i));
    }
    return mkFatMem(memVals);
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
      m_fatMemBaseName("sea.fatmem"),
      m_main(sem, ctx, ptrSz, wordSz, useLambdas),
      m_slots(FatMemSlots, RawMemManager(sem, ctx, ptrSz, g_slotByteWidth, useLambdas)),
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

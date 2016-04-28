#include "seahorn/Harness.hh"

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/ValueMap.h"

#include <memory>

using namespace llvm;
namespace seahorn
{

  Constant* exprToLlvm (IntegerType *ty, Expr e)
  {
    if (isOpX<TRUE> (e))
      return ConstantInt::getTrue (ty);
    else if (isOpX<FALSE> (e))
      return ConstantInt::getFalse (ty);
    else if (isOpX<MPZ> (e))
    {
      mpz_class mpz = getTerm<mpz_class> (e);
      return ConstantInt::get (ty, mpz.get_str (), 10);
    }
    else
    {
      // if all fails, try 0
      LOG("cex", errs () << "WARNING: Not handled value: " << *e << "\n";);
      return ConstantInt::get (ty, 0);
    }
  }

  std::unique_ptr<Module>  createLLVMHarness(BmcTrace &trace)
  {

    std::unique_ptr<Module> Harness(new Module("harness", getGlobalContext()));

    ValueMap<Function*, ExprVector> FuncValueMap;

    // Look for calls in the trace
    for (unsigned loc = 0; loc < trace.size(); loc++)
    {
      const BasicBlock &BB = *trace.bb(loc);
      for (auto &I : BB)
      {
        if (const CallInst *ci = dyn_cast<CallInst> (&I))
        {
          Function *CF = ci->getCalledFunction ();
          if (!CF) continue;

          Expr V = trace.eval (loc, I);
          if (!V) continue;

          // If the function name does not have a period in it,
          // we assume it is an original function.
          if (CF->hasName() &&
              CF->getName().find_first_of('.') == StringRef::npos &&
              CF->isExternalLinkage(CF->getLinkage())) {
            FuncValueMap[CF].push_back(V);
          }
        }
      }
    }

    // Build harness functions
    for (auto CFV : FuncValueMap) {

      auto CF = CFV.first;
      auto& values = CFV.second;

      // This is where we will build the harness function
      Function *HF = cast<Function> (Harness->getOrInsertFunction(CF->getName(), cast<FunctionType> (CF->getFunctionType())));

      IntegerType *RT = dyn_cast<IntegerType> (CF->getReturnType());
      if (!RT)
      {
        errs () << "Skipping non-integer function: " << CF->getName () << "\n";
        continue;
      }


      ArrayType* AT = ArrayType::get(RT, values.size());

      // Convert Expr to LLVM constants
      SmallVector<Constant*, 20> LLVMarray;
      std::transform(values.begin(), values.end(), std::back_inserter(LLVMarray),
                     [RT](Expr e) { return exprToLlvm(RT, e); });

      // This is an array containing the values to be returned
      GlobalVariable* CA = new GlobalVariable(*Harness,
                                              AT,
                                              true,
                                              GlobalValue::PrivateLinkage,
                                              ConstantArray::get(AT, LLVMarray));
      Type *CAType = CA->getType();

      // Build the body of the harness function
      BasicBlock *BB = BasicBlock::Create(getGlobalContext(), "entry", HF);
      IRBuilder<> Builder(BB);

      Type *CountType = Type::getInt32Ty (getGlobalContext());
      GlobalVariable* Counter = new GlobalVariable(*Harness,
                                                   CountType,
                                                   false,
                                                   GlobalValue::PrivateLinkage,
                                                   ConstantInt::get(CountType, 0));

      Value *LoadCounter = Builder.CreateLoad(Counter);
      //Value* Idx[] = {ConstantInt::get(CountType, 0), LoadCounter};
      //Value *ArrayLookup = Builder.CreateLoad(Builder.CreateInBoundsGEP(CA, Idx));

      Value* Args[] = {LoadCounter, CA, ConstantInt::get(CountType, values.size())};
      Type* ArgTypes[] = {CountType, CAType, CountType};

      Builder.CreateStore(Builder.CreateAdd(LoadCounter,
                                            ConstantInt::get(CountType, 1)),
                          Counter);

      std::string RS;
      llvm::raw_string_ostream RSO(RS);
      RT->print(RSO);
      Function *GetValue = llvm::Function::Create(llvm::FunctionType::get(RT, ArgTypes),
                                                  GlobalValue::ExternalLinkage,
                                                  Twine("get_value_").concat(RSO.str()),
                                                  Harness.get());
      Value* RetValue = Builder.CreateCall(GetValue, Args);

      Builder.CreateRet(RetValue);
    }

    return (Harness);
  }
}

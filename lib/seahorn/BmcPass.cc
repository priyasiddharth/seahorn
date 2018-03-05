/**
SeaHorn Verification Framework
Copyright (c) 2016 Carnegie Mellon University.
All Rights Reserved.

THIS SOFTWARE IS PROVIDED "AS IS," WITH NO WARRANTIES
WHATSOEVER. CARNEGIE MELLON UNIVERSITY EXPRESSLY DISCLAIMS TO THE
FULLEST EXTENT PERMITTEDBY LAW ALL EXPRESS, IMPLIED, AND STATUTORY
WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND
NON-INFRINGEMENT OF PROPRIETARY RIGHTS.

Released under a modified BSD license, please see license.txt for full
terms.

DM-0002198
*/

#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Analysis/TargetLibraryInfo.h" 

#include "seahorn/config.h"

#include "ufo/Smt/EZ3.hh"
#include "ufo/Stats.hh"
#include "ufo/Passes/NameValues.hpp"

#include "seahorn/Bmc.hh"
#include "seahorn/PathBasedBmc.hh" 
#include "seahorn/UfoSymExec.hh"
#include "seahorn/BvSymExec.hh"
#include "seahorn/Analysis/CanFail.hh"
// prerequisite for CrabLlvm
#include "seahorn/Transforms/Scalar/LowerCstExpr.hh"

#ifdef HAVE_CRAB_LLVM
#include "crab_llvm/CrabLlvm.hh"
#endif 

namespace
{
  using namespace llvm;
  using namespace seahorn;
  using namespace ufo;
  
  class BmcPass : public llvm::ModulePass
  {
    /// bmc engine
    bmc_engine_t m_engine;
    /// output stream for encoded bmc problem
    raw_ostream *m_out;
    /// true if to run the solver, false if encode only
    bool m_solve;
  public:
    static char ID;
    
    BmcPass (bmc_engine_t engine = mono_bmc, raw_ostream *out = nullptr, bool solve = true) :
      llvm::ModulePass (ID), m_engine(engine), m_out(out), m_solve (solve) {}
    
    virtual bool runOnModule (Module &M)
    {
      for (Function &F : M)
        if (F.getName ().equals ("main")) return runOnFunction (F);
      return false;
    }
    
    void getAnalysisUsage (AnalysisUsage &AU) const
    {
      AU.setPreservesAll ();
      
      AU.addRequired<seahorn::CanFail> ();
      AU.addRequired<ufo::NameValues>();
      AU.addRequired<seahorn::TopologicalOrder>();
      AU.addRequired<CutPointGraph> ();
      AU.addRequired<TargetLibraryInfoWrapperPass> ();
      #ifdef HAVE_CRAB_LLVM
      AU.addRequired<seahorn::LowerCstExprPass>();
      AU.addRequired<crab_llvm::CrabLlvmPass>();
      #endif 
    }      

    virtual bool runOnFunction (Function &F)
    {
      
      const CutPointGraph &cpg = getAnalysis<CutPointGraph> (F);
      const CutPoint &src = cpg.getCp (F.getEntryBlock ());
      const CutPoint *dst = nullptr;
    
      // -- find return instruction. Assume it is unique
      for (auto &bb : F)
        if (llvm::isa<llvm::ReturnInst> (bb.getTerminator ()) && cpg.isCutPoint (bb))
        {
          dst = &cpg.getCp (bb);
          break;
        }

      if (dst == nullptr) {
	//cpg.print(llvm::errs (), F.getParent());
	errs () << "WARNING: BmcPass: function '" << F.getName() << "' never returns\n";
	return false;
      }
      
      if (!cpg.getEdge (src, *dst)) {
	//cpg.print(llvm::errs (), F.getParent());	
	errs () << "WARNING: BmcPass: function '" << F.getName() << "' never returns\n";	
	return false;
      }
      
      ExprFactory efac;
      BvSmallSymExec sem (efac, *this, F.getParent()->getDataLayout(), MEM);
      
      EZ3 zctx (efac);
      std::unique_ptr<BmcEngine> bmc;
      switch(m_engine) {
      case path_bmc: {
	const TargetLibraryInfo &tli = getAnalysis<TargetLibraryInfoWrapperPass> ().getTLI();
	#ifdef HAVE_CRAB_LLVM
	crab_llvm::CrabLlvmPass &crab = getAnalysis<crab_llvm::CrabLlvmPass> ();
	bmc.reset(new PathBasedBmcEngine(sem, zctx, &crab, tli));
	#else
	bmc.reset(new PathBasedBmcEngine(sem, zctx, tli));
	#endif 
	break;
      }
      case mono_bmc:
      default:
	bmc.reset(new BmcEngine(sem, zctx));
      }

      
      bmc->addCutPoint (src);
      bmc->addCutPoint (*dst);
      LOG("bmc", errs () << "BMC from: " << src.bb ().getName ()
          << " to " << dst->bb ().getName () << "\n";);
      
      bmc->encode ();
      if (m_out) bmc->toSmtLib (*m_out);
      
      if (!m_solve)
        {
          LOG ("bmc", errs () << "Stopping before solving\n";);
          return false;
        }
      
      Stats::resume ("BMC");
      auto res = bmc->solve ();
      Stats::stop ("BMC");
     
      if (res) outs () << "sat";
      else if (!res) outs () << "unsat";
      else outs () << "unknown";
      outs () << "\n";
      
      if (res) Stats::sset ("Result", "FALSE");
      else if (!res) Stats::sset ("Result", "TRUE");
      
      LOG ("bmc",
           if (!res) {
	     ExprVector core;
	     bmc->unsatCore (core);
	     errs () << "CORE BEGIN\n";
	     for (auto c : core) errs () << *c << "\n";
	     errs () << "CORE END\n";
	   }
           );
      
      LOG ("cex", 
           if (res) 
             {
               errs () << "Analyzed Function:\n" << F << "\n";
               BmcTrace trace (bmc->getTrace ());
	       errs () << "Trace \n";
               trace.print (errs ());
             });
      
      return false;
    }
    
    virtual const char *getPassName () const {return "BmcPass";}
    
    
  };

  char BmcPass::ID = 0;
}
namespace seahorn
{
  Pass *createBmcPass (bmc_engine_t engine, raw_ostream *out, bool solve)
  {return new BmcPass (engine, out, solve);}
}

static llvm::RegisterPass<BmcPass>
X("bmc-pass", "Run BMC engine");



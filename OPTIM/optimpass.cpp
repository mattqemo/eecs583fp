#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Format.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/BlockFrequencyInfo.h"
#include "llvm/Analysis/BranchProbabilityInfo.h"
#include "llvm/Analysis/Loads.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopIterator.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Analysis/MemoryLocation.h"
#include "llvm/Transforms/Utils/SSAUpdater.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Constants.h"

#include "../ANALYSIS/analysispass.cpp"

#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <fstream>
#include <utility>

using namespace llvm;

/*
PLAN:
  - pure function optimization
  - maybe also LICM (see if can disable register promotion)

Note: can examine compiled code with
https://stackoverflow.com/questions/10990018/how-to-generate-assembly-code-with-clang-in-intel-syntax
*/

namespace {

struct FuncCallsAliasProfilePass : public ModulePass {
  static char ID;
  double aliasProbaThreshold = 0.80;

  FuncCallsAliasProfilePass() : ModulePass(ID) {}

  void getAnalysisUsage(AnalysisUsage& AU) const override {
    AU.addRequired<fp583::InstLogAnalysisWrapperPass>();
  }

  MemoryLocation getMemLocFromPtr(const Value* val) {
    return MemoryLocation(val); // TOCHECK: this is jank (this should work bc analysis just looks at ptr value but in practice it's bad style)
  }

  bool areFunctionCallsIdentical(const fp583::InstLogAnalysis& instLogAnalysis, CallBase* call1, CallBase* call2, std::vector<std::pair<Value*, Value*>>& ptrArgsVals){
    assert(call1->getCalledFunction() == call2->getCalledFunction());
    auto* calledF = call1->getCalledFunction();
    assert(calledF->getName().contains("_PURE_") && "function name contains PURE");

    for (unsigned int i = 0; i < calledF->arg_size(); ++i) {
      auto* arg = calledF->getArg(i);
      auto* val1 = call1->getArgOperand(i);
      auto* val2 = call2->getArgOperand(i);


      if (val1 == val2) {
        return true;
      }
      else if (auto* loadInst1 = dyn_cast<LoadInst>(val1), *loadInst2 = dyn_cast<LoadInst>(val2); loadInst1 && loadInst2) {
        auto memLoc1 = MemoryLocation::get(loadInst1), memLoc2 = MemoryLocation::get(loadInst2);
        double probaAlias = instLogAnalysis.getAliasProbability(memLoc1, memLoc2);
        if (probaAlias < aliasProbaThreshold) {
          return false;
        }
        ptrArgsVals.push_back({val1, val2});
      }
      else return false;
    }
    return true;
  }

  bool isFunctionPure(Function* f) {
    return f->getName().contains("_PURE_");
  }


  Instruction* generateFixUpICmp(CallBase* ogCall, const std::vector<std::pair<Value*, Value*>>& ptrArgsVals) {
    Instruction* lastComp = nullptr;
    for (auto& [val1, val2] : ptrArgsVals) {
      auto* valComp = new ICmpInst(ogCall, ICmpInst::ICMP_NE, val1, val2);
      if (lastComp) {
        lastComp = BinaryOperator::CreateOr(valComp, lastComp);
        lastComp->insertBefore(ogCall);
      }
      else {
        lastComp = valComp;
      }
    }
    assert(lastComp);
    return lastComp;
  }

  /* returns fixUpBB so we can exclude it from our optimization pass (would loop forever otherwise) */
  BasicBlock* removeFunctionCallAndFixUp(CallBase* ogCall, CallBase* prevCall, const std::vector<std::pair<Value*, Value*>>& ptrArgsVals) {
    Instruction* lastComp = generateFixUpICmp(ogCall, ptrArgsVals);

    auto* currBB = ogCall->getParent();
    auto* followingBB = currBB->splitBasicBlock(ogCall);
    currBB->getInstList().back().eraseFromParent(); // erase unconditional branch added by splitBasicBlock

    auto* f = currBB->getParent();
    auto* fixUpBB = BasicBlock::Create(f->getContext(), "", f);
    auto* condCheckToFixUpBranch = BranchInst::Create(fixUpBB, followingBB, lastComp, currBB);
    auto* fixUpEndBranch = BranchInst::Create(followingBB, fixUpBB);

    auto* callInFixUp = ogCall->clone();
    callInFixUp->insertBefore(fixUpBB->getTerminator());

    if (!ogCall->getType()->isVoidTy()) {
      auto* phiNode = PHINode::Create(ogCall->getType(), 2, "", ogCall);
      phiNode->addIncoming(prevCall, currBB);
      phiNode->addIncoming(callInFixUp, fixUpBB);
      ogCall->replaceAllUsesWith(phiNode);
    }

    ogCall->eraseFromParent();
    return fixUpBB;
  }

  /* example:
    // main()
    //  int val1 = 5;
    //  int val2 = 6;
    //   fn(val1, ptrA);
    //    --> fn(val1, ptrB);
    //   fn(val2, ptrB);
  */
  /* do actual optimizations */
  bool handleFunction(const fp583::InstLogAnalysis& instLogAnalysis, Function& f) {
    bool changed = false;
    std::unordered_map<Function*, std::vector<CallBase*>> prevFunctionCalls;
    std::unordered_set<BasicBlock*> fixUpBBs; /* We need to avoid fixUpBBs we introduce as
    they contain calls that can be optimized (otherwise they wouldn't be in fixUp) and if we don't avoid
    them, our loop over BBs goes forever */

    for (auto& bb : f) {
      if (fixUpBBs.count(&bb)) {
        fixUpBBs.erase(&bb);
        continue;
      }

      auto* nextInst = &bb.getInstList().front();
      while (auto* inst = nextInst) {
        nextInst = inst->getNextNode();

        if (auto* currCall = dyn_cast<CallBase>(inst)) {
          auto* fCalled = currCall->getCalledFunction();
          if (!isFunctionPure(fCalled)) continue;

          bool callNotDeleted = true;
          if (prevFunctionCalls.count(fCalled)) {
            auto& prevCallsVec = prevFunctionCalls[fCalled];
            for (auto* prevCall : prevCallsVec) {
              std::vector<std::pair<Value*, Value*>> ptrArgsVals;

              // check !ptrArgsVals.empty() to limit scope to only calls with ptr input
              if (areFunctionCallsIdentical(instLogAnalysis, currCall, prevCall, ptrArgsVals) && !ptrArgsVals.empty()) {
                // errs() << "optim function call: " << fCalled->getName() << "\n";
                auto* fixUpBB = removeFunctionCallAndFixUp(currCall, prevCall, ptrArgsVals);
                fixUpBBs.insert(fixUpBB);
                changed = true;
                callNotDeleted = false;
                break;
              }
            }
          }

          if (callNotDeleted) prevFunctionCalls[fCalled].push_back(currCall);
          else break;
        }
      }
    }

    return changed;
  }

  bool runOnModule(Module &m) override {
    bool changed = false;
    auto& instLogAnalysis = getAnalysis<fp583::InstLogAnalysisWrapperPass>().getInstLogAnalysis();

    for (auto& f : m) {
      changed |= handleFunction(instLogAnalysis, f);
    }

    return changed;
  }
}; // end of struct OptOnAliasProfilePass


/* ****************************************************************** */

// LICM PASS

/* ****************************************************************** */


struct LICMAliasProfilePass : public LoopPass {
  static char ID;
  double aliasProbaThreshold = 0.02;


  LICMAliasProfilePass() : LoopPass(ID) {}

  void getAnalysisUsage(AnalysisUsage& AU) const override {
    AU.addRequired<fp583::InstLogAnalysisWrapperPass>();
    AU.addRequired<AAResultsWrapperPass>();
  }

  llvm::DenseMap<LoadInst*, std::vector<StoreInst*>> getMostlyInvariantLoads(Loop *L) {
    auto& instLogAnalysis = getAnalysis<fp583::InstLogAnalysisWrapperPass>().getInstLogAnalysis();
    auto& aliasResults = getAnalysis<AAResultsWrapperPass>().getAAResults();
    llvm::DenseMap<LoadInst*, std::vector<StoreInst*>> hoistLoadsToStores;

    for (auto* bb : L->getBlocks()) {
      for (auto& inst : *bb) {
        if (auto* loadInst = dyn_cast<LoadInst>(&inst)) {
          hoistLoadsToStores[loadInst] = {}; // TODO: init with all stores in loop
        }
      }
    }

    /*
    start with  stores to fix up = ALL
    for every store and loads to hoist:
      if is MUST alias || (proba alias > threshold) --> remove load to hoist from set (shouldn't hoist)
      if is WILL NOT alias --> remove store from dependent stores vec (no fix up ever needed)
    */
    for (auto* bb : L->getBlocks()) {
      for (auto& inst : *bb) {
        if (auto* storeInst = dyn_cast<StoreInst>(&inst)) {
          llvm::remove_if(hoistLoadsToStores, [&](const auto& loadInstAndStore) {
            auto memLoc1 = MemoryLocation::get(loadInstAndStore.first), memLoc2 = MemoryLocation::get(storeInst);
            return
              aliasResults.isMustAlias(memLoc1, memLoc2) ||
              instLogAnalysis.getAliasProbability(memLoc1, memLoc2) > aliasProbaThreshold;
          });
        }
      }
    }
    return hoistLoadsToStores;
  }

  bool runOnLoop(Loop *L, LPPassManager &LPM) override {
    bool changed = false;

    /* 1. use the log analysis to determine if there are any loads worth hoisting */
    auto mostlyInvariantLoads = getMostlyInvariantLoads(L);
    for (auto& [loadInst, dependentStores] : mostlyInvariantLoads) {
      loadInst->moveBefore(L->getLoopPreheader()->getTerminator());
    }

    return changed;
  }
}; // end of struct LICMAliasProfilePass
}  // end of anonymous namespace

char FuncCallsAliasProfilePass::ID = 0;
char LICMAliasProfilePass::ID = 0;
static RegisterPass<FuncCallsAliasProfilePass> x("fp_funcoptim", "FuncCallsAliasProfilePass Pass",
                             false /* Only looks at CFG */,
                             false /* Analysis Pass */);
static RegisterPass<LICMAliasProfilePass> xx("fp_licmoptim", "LICMAliasProfilePass Pass",
                             false /* Only looks at CFG */,
                             false /* Analysis Pass */);

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

  template <typename INST_T, typename BB_CONTAINER_T>
  void forEachInstOfType(const BB_CONTAINER_T& bbs, const std::function<void(INST_T*)>& action) {
    for (auto* bb : bbs)
      for (auto& inst: *bb)
        if (auto* typedInst = dyn_cast<INST_T>(&inst))
          action(typedInst);
  }

  /*
    Return a map from mostly invariant loads to stores which might alias with them
    Loads which are statically determined to be completely or never invariant are not returned
  */
  llvm::DenseMap<LoadInst*, std::vector<StoreInst*>> getMostlyInvariantLoads(Loop *L) {
    auto& instLogAnalysis = getAnalysis<fp583::InstLogAnalysisWrapperPass>().getInstLogAnalysis();
    auto& aliasResults = getAnalysis<AAResultsWrapperPass>().getAAResults();
    llvm::DenseMap<LoadInst*, std::vector<StoreInst*>> hoistLoadsToStores;

    // Collect all stores
    auto allStores = std::vector<StoreInst*>{};
    forEachInstOfType<StoreInst>(L->getBlocks(), [&allStores](auto* storeInst){ allStores.push_back(storeInst); });
    // All loads map to all stores initially
    // TODO: I think we need to check first that the load is constant
    forEachInstOfType<LoadInst>(L->getBlocks(), [&hoistLoadsToStores, &allStores](auto* loadInst){ hoistLoadsToStores[loadInst] = allStores; });

    // Remove any loads which must-alias with any stores, or which are too likely to alias
    llvm::remove_if(hoistLoadsToStores, [&](const auto& loadInstAndStores) {
      return llvm::any_of(loadInstAndStores.second, [&](auto* storeInst) {
        auto memLoc1 = MemoryLocation::get(loadInstAndStores.first), memLoc2 = MemoryLocation::get(storeInst);
        return aliasResults.isMustAlias(memLoc1, memLoc2) || instLogAnalysis.getAliasProbability(memLoc1, memLoc2) > aliasProbaThreshold;
      });
    });

    // Remove any load/store pairs which are known to never alias
    for (auto& loadInstAndStores : hoistLoadsToStores) {
      llvm::remove_if(loadInstAndStores.second, [&](auto* storeInst) {
        return aliasResults.isNoAlias(MemoryLocation::get(loadInstAndStores.first), MemoryLocation::get(storeInst));
      });
    }

    llvm::remove_if(hoistLoadsToStores, [&](const auto& loadInstAndStores) {
      return loadInstAndStores.second.empty();
    });

    return hoistLoadsToStores;
  }

  void insertFixUpCode(Loop *L, LoadInst* loadInst, StoreInst* storeInst, LoadInst* headerLoadValue) {
    auto* compForAlias = new ICmpInst(*storeInst->getParent(), ICmpInst::ICMP_EQ, storeInst->getPointerOperand(), loadInst->getPointerOperand());

    auto* storeBB = storeInst->getParent();
    auto* followingBB = storeBB->splitBasicBlock(storeInst);
    storeBB->getInstList().back().eraseFromParent(); // erase unconditional branch added by splitBasicBlock

    auto* f = storeBB->getParent();
    auto* fixUpBB = BasicBlock::Create(f->getContext(), "", f);
    auto* condCheckToFixUpBranch = BranchInst::Create(fixUpBB, followingBB, compForAlias, storeBB);
    auto* fixUpEndBranch = BranchInst::Create(followingBB, fixUpBB);

    auto* fixUpStore = new StoreInst(storeInst->getValueOperand(), headerLoadValue, fixUpBB);
  }

  void hoistLoadAndInsertFixUp(Loop *L, LoadInst* loadInst, const std::vector<StoreInst*> dependentStores) {
    auto* function = loadInst->getParent()->getParent();
    auto& entryBB = function->getEntryBlock();

    /* Function Entry Block */
    auto* loadValueVar = new AllocaInst(loadInst->getType(), 0, nullptr, "", &(*entryBB.begin()));

    /* Loop Preheader */
    auto* initVal = new LoadInst(loadInst, "", L->getLoopPreheader()->getTerminator());
    auto* storeInitVal = new StoreInst(initVal, loadValueVar, L->getLoopPreheader()->getTerminator());

    /* Loop header */
    auto* headerLoadValue = new LoadInst(loadValueVar, "", loadInst);

    for (auto* storeInst : dependentStores) {
      insertFixUpCode(L, loadInst, storeInst, headerLoadValue);
    }
  }

  bool runOnLoop(Loop *L, LPPassManager &LPM) override {
    bool changed = false;

    /* 1. use the log analysis to determine if there are any loads worth hoisting */
    auto mostlyInvariantLoads = getMostlyInvariantLoads(L);
    for (auto& [loadInst, dependentStores] : mostlyInvariantLoads) {
      errs() << "load is mostly invariant: " << *loadInst << "\n";
      hoistLoadAndInsertFixUp(L, loadInst, dependentStores);
      // TODO: hoist load inst to preheader
      //      hoist dependent instructions (math)
      //      add re-calculation code in fixup when a store to the same pointer happens
      // loadInst->moveBefore(L->getLoopPreheader()->getTerminator());
    }

    return changed;
  }
}; // end of struct LICMAliasProfilePass
}  // end of anonymous namespace

char FuncCallsAliasProfilePass::ID = 0;
char LICMAliasProfilePass::ID = 1;
static RegisterPass<FuncCallsAliasProfilePass> x("fp_funcoptim", "FuncCallsAliasProfilePass Pass",
                             false /* Only looks at CFG */,
                             false /* Analysis Pass */);
static RegisterPass<LICMAliasProfilePass> xx("fp_licmoptim", "LICMAliasProfilePass Pass",
                             false /* Only looks at CFG */,
                             false /* Analysis Pass */);

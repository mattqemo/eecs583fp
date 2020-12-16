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
#include <cassert>
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

bool isFunctionPure(Function* f) {
  return f->getName().contains("_PURE_");
}

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
    AU.addRequired<LoopInfoWrapperPass>();
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
  std::map<LoadInst*, std::vector<StoreInst*>> getMostlyInvariantLoads(Loop *L) {
    auto& instLogAnalysis = getAnalysis<fp583::InstLogAnalysisWrapperPass>().getInstLogAnalysis();
    auto& aliasResults = getAnalysis<AAResultsWrapperPass>().getAAResults();
    std::map<LoadInst*, std::vector<StoreInst*>> hoistLoadsToStores;

    // Collect all stores
    auto allStores = std::vector<StoreInst*>{};
    forEachInstOfType<StoreInst>(L->getBlocks(), [&allStores](auto* storeInst){ allStores.push_back(storeInst); });
    // All loads map to all stores initially
    forEachInstOfType<LoadInst>(L->getBlocks(), [this, L, &hoistLoadsToStores, &allStores](auto* loadInst){
      auto* ptrOp = loadInst->getPointerOperand();
      auto* loadOp = dyn_cast<LoadInst>(ptrOp);
      // check if pointer is invariant, if result from a load, check if it'll be hoisted
      if (loadOp && hoistLoadsToStores.count(loadOp) || L->isLoopInvariant(ptrOp)) {
        hoistLoadsToStores[loadInst] = allStores; 
      }
    });

    // Remove any loads which must-alias with any stores, or which are too likely to alias
    for (auto it = begin(hoistLoadsToStores); it != end(hoistLoadsToStores); ) {
      if (llvm::any_of(it->second, [&](auto* storeInst) {
        auto memLoc1 = MemoryLocation::get(it->first), memLoc2 = MemoryLocation::get(storeInst);
        return aliasResults.isMustAlias(memLoc1, memLoc2) || instLogAnalysis.getAliasProbability(memLoc1, memLoc2) > aliasProbaThreshold;
      })) {
        it = hoistLoadsToStores.erase(it);
      }
      else ++it;
    } // can't use remove_if on std::map, and llvm::remove_if doesn't erase, and a map doesn't have range-based erase

    // Remove any load/store pairs which are known to never alias
    for (auto& [loadInst, dependentStores] : hoistLoadsToStores) {
      auto end_it = llvm::remove_if(dependentStores, [&](auto* storeInst) {
        return aliasResults.isNoAlias(MemoryLocation::get(loadInst), MemoryLocation::get(storeInst))
            || (loadInst->getPointerOperand()->getType() != storeInst->getPointerOperand()->getType()); 
            // TOCHECK: not great, assumes no alias of loads to different types, but makes thing easier
      });
      
      dependentStores.erase(end_it, dependentStores.end());
    } 

    return hoistLoadsToStores;
  }

  void fixUpForStore(Loop *L, LoadInst* ogLoadInst, StoreInst* storeInst, AllocaInst* hoistedValue, CallBase* call, Value* ogPtrOp, LoopInfo* LI) {
    auto* compForAlias = new ICmpInst(storeInst->getNextNode(), ICmpInst::ICMP_EQ, storeInst->getPointerOperand(), ogPtrOp);
    auto* storeBB = storeInst->getParent();
    auto* followingBB = storeBB->splitBasicBlock(compForAlias->getNextNode());
    L->addBasicBlockToLoop(followingBB, *LI);
    storeBB->getInstList().back().eraseFromParent(); // erase unconditional branch added by splitBasicBlock
    
    auto* f = storeBB->getParent();
    auto* fixUpBB = BasicBlock::Create(f->getContext(), "", f);
    L->addBasicBlockToLoop(fixUpBB, *LI);
    auto* condCheckToFixUpBranch = BranchInst::Create(fixUpBB, followingBB, compForAlias, storeBB);
    auto* fixUpEndBranch = BranchInst::Create(followingBB, fixUpBB);

    auto* loadParam = new LoadInst(ogPtrOp, "", fixUpBB->getTerminator());
    auto* callVal = CallInst::Create(call->getFunctionType(), call->getCalledFunction(), {loadParam}, "", fixUpBB->getTerminator());
    auto* fixUpStore = new StoreInst(callVal, hoistedValue, fixUpBB->getTerminator());
  }

  /* We only hoist loads and dependent pure function calls for this example
    Not all loads have dependent pure function calls, so we handle the 2 cases a bit differently
  */
  void hoistLoadAndInsertFixUp(Loop *L, LoadInst* ogLoadInst, const std::vector<StoreInst*> dependentStores, std::unordered_map<LoadInst*, Value*>& loopLoadToHoisted, LoopInfo* LI) {
    CallBase* call = nullptr;
    for(auto* U : ogLoadInst->users()){  // find the dependent pure func call
      if (auto* callBase = dyn_cast<CallBase>(U)){
        if (isFunctionPure(callBase->getCalledFunction())) {
          call = callBase;
          break;
        }
      }
    }
      
    auto* function = ogLoadInst->getParent()->getParent();
    auto& entryBB = function->getEntryBlock();

    /* Function Entry Block */
    auto* hoistedType = call ? call->getType() : ogLoadInst->getType();
    auto* hoistedValue = new AllocaInst(hoistedType, 0, nullptr, "", &(*entryBB.begin()));

    /* Loop Preheader */
    Instruction* initVal;
    LoadInst* initLoad;

    /* this part is needed because the load for a pure func arg can be dependent on another loop invariant loads, which
       would have already been hoisted -> ptrOperand used in pre-header has to be updated */
    auto* loadPtrOp = ogLoadInst->getPointerOperand();
    auto* ogPtrOp = loopLoadToHoisted.count(dyn_cast<LoadInst>(loadPtrOp)) ? loopLoadToHoisted[dyn_cast<LoadInst>(loadPtrOp)] : loadPtrOp;
    
    if (call) {
      initLoad = new LoadInst(ogPtrOp, "", L->getLoopPreheader()->getTerminator());
      auto* calledFunction = call->getCalledFunction();
      initVal = CallInst::Create(calledFunction->getFunctionType(), calledFunction, {initLoad}, "", L->getLoopPreheader()->getTerminator());
    }
    else {
      initVal = new LoadInst(ogPtrOp, "",  L->getLoopPreheader()->getTerminator());
    }
    auto* storeInitVal = new StoreInst(initVal, hoistedValue, L->getLoopPreheader()->getTerminator());

    /* Loop header */
    auto* headerLoadValue = new LoadInst(hoistedValue, "", ogLoadInst);

    for (auto* storeInst : dependentStores) {
      fixUpForStore(L, ogLoadInst, storeInst, hoistedValue, call, ogPtrOp, LI);
    }

    if (call) {
      call->replaceAllUsesWith(headerLoadValue);
      call->eraseFromParent();
    }
    else {
      ogLoadInst->replaceAllUsesWith(headerLoadValue);
      loopLoadToHoisted[headerLoadValue] = initVal;
    }
    ogLoadInst->eraseFromParent();
  }

  bool runOnLoop(Loop *L, LPPassManager &LPM) override {
    // if (L->getBlocks().front()->getParent()->getName() != "main") return false;
    auto mostlyInvariantLoads = getMostlyInvariantLoads(L);
    std::unordered_map<LoadInst*, Value*> loopLoadToHoisted;
    auto* LI = &getAnalysis<LoopInfoWrapperPass>().getLoopInfo();

    for (auto& [loadInst, dependentStores] : mostlyInvariantLoads) {
      hoistLoadAndInsertFixUp(L, loadInst, dependentStores, loopLoadToHoisted, LI);
    }

    return !mostlyInvariantLoads.empty();
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

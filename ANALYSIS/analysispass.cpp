#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Format.h"
#include "llvm/Analysis/BlockFrequencyInfo.h"
#include "llvm/Analysis/BranchProbabilityInfo.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/MemoryLocation.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Constants.h"
#include <array>

#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <fstream>
#include <type_traits>

using namespace llvm;

/*
TODO: address potential issue that our profile data might be invalidated by other transforming passes
running BEFORE our last pass.
*/

/*
    auto *SE = getAnalysisIfAvailable<ScalarEvolutionWrapperPass>();
    MemorySSA *MSSA = EnableMSSALoopDependency
                          ? (&getAnalysis<MemorySSAWrapperPass>().getMSSA())
                          : nullptr;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<BranchProbabilityInfoWrapperPass>();
    AU.addRequired<BlockFrequencyInfoWrapperPass>();
    AU.addPreserved<DominatorTreeWrapperPass>();
    AU.addRequired<DominanceFrontierWrapperPass>();
    AU.addPreserved<LoopInfoWrapperPass>();
    AU.addRequired<TargetLibraryInfoWrapperPass>();
    if (EnableMSSALoopDependency) {
      AU.addRequired<MemorySSAWrapperPass>();
      AU.addPreserved<MemorySSAWrapperPass>();
    }
    AU.addRequired<TargetTransformInfoWrapperPass>();
    getLoopAnalysisUsage(AU);
  }
*/
/*

*/

namespace {

struct InstLogAnalysis {
  std::vector<Instruction*> insts;
  // struct MemPairKey {
  //   const MemoryLocation& loc_a;
  //   const MemoryLocation& loc_b;
  // }
  // std::unordered_map<std::pair<MemoryLocation>>


  // { std::vector<std::pair<void*, u_int32_t>>, size }
  // struct s{ std::map<void* /*ptr value*/, uint32_t/*value freq*/>, int accessSize };
  // std::unordered_map<MemoryLocation*, s>

  /*
issues:
  might detect "fake"/"useless?" aliasing across iterations/function calls
  one fix would be to change interface to doAlias(ptrA, ptrB, %i) where i is where "start looking";

loop:
    %1: load(ptrA);      ptrA==0x1
    %2: load(ptrB);      ptrB==0x2
loop:
    %1: load(ptrA);      ptrA==0x2
    doAlias(ptrA, ptrB, i);
    %2: load(ptrB);      ptrA==0x3
loop:
    %1: load(ptrA);      ptrA==0x3
    %2: load(ptrB);      ptrA==0x4

    %3: ptrC = 0x1


fn:
    %1: load(ptrA);      ptrA==0x1
    %2: load(ptrB);      ptrB==0x2
    %3: store(ptrB);
fn:
    %1: load(ptrA);      ptrA==0x2
    %2: load(ptrB);      ptrA==0x3

  */
 // ^ DOESN'T alias
  /*
    ptrA = 0x1;
    ptrB = 0x1;
  */
 // ^ DOES alias
 /*
  ptrToVal
  valToPtr

  when ptr is assigned new value -> check if that value aliases with any other ptr

  what have we learned?
    memory location prob not unique to an inst?
    need to fix our stuff
 */

  double getAliasProbability(const MemoryLocation& loc_a, const MemoryLocation& loc_b) {
    auto keya = getKey(loc_a);
    auto keyb = getKey(loc_b);
  }
  void printInsts() {
    for (uint32_t i = 0; i < insts.size(); i++){
      errs() << i << " " << *insts[i] << "\n";
    }
  }
  // void addInst(Instruction* inst) { insts.push_back(inst); }
};


struct InstLogAnalysisWrapperPass : public ModulePass {
  static char ID;

  InstLogAnalysisWrapperPass() : ModulePass(ID) {}

  bool runOnModule(Module &m) override {
    // run thru and do mapping (for bb : for func ...)
    auto* instLogFunc = m.getFunction("_inst_log");
    assert(instLogFunc && "instLogFunc not found");

    for (auto& func : m) {
      if (&func == instLogFunc) continue;
      for (auto& bb : func) {
        for (auto& inst : bb) {
          if (isa<LoadInst>(inst) or isa<StoreInst>(inst)) {
            instLogAnalysis.insts.push_back(&inst);
            // instLogAnalysis.addInst(&inst);
          }
        }
      }
    }

    instLogAnalysis.printInsts();

    // read in from log & do analysis on values
    /*
    Sample line:
      6 (instrID)
      0x7ffda4e5109c (addr)
      4 (size of pointer)
      S (store or load)
      main (function name)
    */

    uint32_t instID;
    uint64_t memAddr;
    uint32_t ptrSize;
    char memType;
    std::string fcnName;
    std::ifstream ins("../583simple/log.log");

    while (ins >> instID >> memAddr >> ptrSize >> memType >> fcnName) {

      // develop a list of [start, end] pairing for each pointers accesses
    }

    // get MemoryLocation from Instruction -> https://llvm.org/doxygen/classllvm_1_1MemoryLocation.html#a61dc6d1a1e9c3cb0adb4c791b329ff31

    // create actual InstLogAnalysis data structures
    return false;
  }
private:
  InstLogAnalysis instLogAnalysis;

}; // end of struct InstLogAnalysisWrapperPass
}  // end of anonymous namespace

char InstLogAnalysisWrapperPass::ID = 0;
static RegisterPass<InstLogAnalysisWrapperPass> X("fp1", "InstLogAnalysisWrapperPass Pass",
                             false /* Only looks at CFG */,
                             false /* Analysis Pass */);

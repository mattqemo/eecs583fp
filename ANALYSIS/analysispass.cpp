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
#include <unordered_set>
#include <fstream>
#include <type_traits>

#include "../PROFILE/helpers.hpp"

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
    %2: load(ptrB);      ptrB==0x3
loop:
    %1: load(ptrA);      ptrA==0x3
    %2: load(ptrB);      ptrB==0x4

    %3: ptrC = 0x1


ptrA = alloca
x = alloca
store(500, %x)
store(600, %x)
y = load %x

int y;
int Y;
int* yPtr = &y;
int* yPtr1 = &Y;
int** yPtrPtr = &yPtr;
*yPtrPtr = &yPtr1;



%y = alloca i32, align 4
(LOG: %y is 0xFFFFF...)
%yPtr = alloca i32*, align 8
(LOG: %yPtr is 0xAAAAA...)
store i32* %y, i32** %yPtr, align 8
%yPtr2 = alloca i32*, align 8

%yPtrPtr = alloca i32**, align 8
(LOG: %yPtrPtr is 0xBBBBB...)
store i32** yPtr2, i32*** yPtrPtr, align 8
%val = alloca ...
%temp = load i32* yPtr2
store temp, val

load(ptrA);      ptrA==0x3
load(ptrB);      ptrB==0x4
load(ptrB);      ptrB==0x5
load(ptrB);      ptrB==0x3
store(ptrB)

1st key is smallest ptr

fn:
    %1: load(ptrA);      ptrA==0x1
    %2: load(ptrB);      ptrB==0x2
    %3: store(ptrB);
fn:
    %1: load(ptrA);      ptrA==0x2
    %2: load(ptrB);      ptrA==0x3

  */
 /*
  ptrToVal
  valToPtr

  when ptr is assigned new value -> check if that value aliases with any other ptr

  what have we learned?
    memory location prob not unique to an inst?
    need to fix our stuff
 */

  double getAliasProbability(const MemoryLocation& loc_a, const MemoryLocation& loc_b) {
    return 0.0;
    // auto keya = getKey(loc_a);
    // auto keyb = getKey(loc_b);
  }
  void printInsts() {
    for (uint32_t i = 0; i < insts.size(); i++){
      errs() << i << " " << *insts[i] << "\n";
    }
  }
  // void addInst(Instruction* inst) { insts.push_back(inst); }
};


struct AliasingStats {
  uint32_t num_collisions;
  uint32_t num_comparisons;

  AliasingStats() : num_collisions(0), num_comparisons(0) {}
};


struct InstLogAnalysisWrapperPass : public ModulePass {
  static char ID;
  std::unordered_map<const Value*, uint32_t> memLocPtrToId; // TO EVALUATE: assume here that the Ptr field of MemoryLocation is unique per MemLoc
  std::unordered_map<uint32_t, MemoryLocation> idToMemLoc;

  InstLogAnalysisWrapperPass() : ModulePass(ID) {}

  void buildIdToMemLocMapping(Module &m, Function* instLogFunc) {
    uint32_t currId = 0;

    for (auto& func : m) {
      if (&func == instLogFunc) continue;
      for (auto& bb : func) {
        for (auto& inst : bb) {
          if (isa<LoadInst>(inst) or isa<StoreInst>(inst)) {
            auto memLoc = MemoryLocation::get(&inst);
            if (!memLocPtrToId.count(memLoc.Ptr)) {
              // errs() << *memLoc.Ptr << ' ' << currId << '\n';
              memLocPtrToId[memLoc.Ptr] = currId;
              idToMemLoc[currId] = memLoc;
              ++currId;
            }
            else {
              // errs() << "already mapped\n";
            }
          }
        }
      }
    }

    assert(memLocPtrToId.size() == idToMemLoc.size() && "memLocPtrToId and idToMemLoc are different size");
  }

  std::unordered_map<uint64_t, AliasingStats> parseLogAndComputerAliasingStats() {
    uint32_t instIdIn;
    void* memAddrIn_void;
    std::ifstream ins("../583simple/log.log");

    std::unordered_map<uint32_t, uint64_t> idToShadowPtr;
    std::unordered_map<uint64_t, AliasingStats> idPairToAliasingStats;
    while (ins >> instIdIn >> memAddrIn_void) {
      uint64_t memAddrIn = (uint64_t)memAddrIn_void;
      assert(idToMemLoc.count(instIdIn));
      idToShadowPtr[instIdIn] = memAddrIn;
      errs() << "hi " << memAddrIn_void << ' ' << memAddrIn << '\n';

      /* TOCHECK: only compare against Memlocs that currently have a shadow value */
      for (auto it_shadow = idToShadowPtr.begin(); it_shadow != idToShadowPtr.end(); ++it_shadow) {
        errs() << "loop ";
        uint32_t instIdCompare = it_shadow->first;
        uint64_t memAddrCompare = it_shadow->second;
        if (instIdCompare == instIdIn) {
          continue; // don't compute aliasing stats with itself
        }

        uint64_t pairKey = getIdPairKey(instIdIn, instIdCompare);
        idPairToAliasingStats[pairKey].num_comparisons++;
        if (memAddrIn == memAddrCompare) {
          idPairToAliasingStats[pairKey].num_collisions++;
          errs() << "\tCOLLISION DETECTED\n";
        }
      }

      // idea: do all pair-wise comparisons using the "shadow" structure holding most recent addresses
    }

    return idPairToAliasingStats;
  }

  /* convention is smaller id is left most in the key */
  uint64_t getIdPairKey(uint32_t id_a, uint32_t id_b) {
    assert(id_a != id_b && "getIdPairKey called with 2 identical IDs as parameters");
    if (id_a > id_b) return getIdPairKey(id_b, id_a);

    errs() << "getIdPairKey: " << id_a << ' ' << id_b << ' ' << (id_a << sizeof(uint32_t)) + id_b << '\n';

    return (id_a << sizeof(uint32_t)) + id_b;
  }

  bool runOnModule(Module &m) override {
    auto* instLogFunc = m.getFunction("_inst_log");
    assert(instLogFunc && "instLogFunc not found");

    buildIdToMemLocMapping(m, instLogFunc);
    errs() << "********\nbuilding maps done\n\n";

    std::unordered_map<uint64_t, AliasingStats> idToAliasingStats = parseLogAndComputerAliasingStats();

    errs() << "********\nparsing done \n\n";

    // for (auto& func : m) {
    //   for (auto& bb : func) {
    //     for (auto& inst : bb) {
    //       if (isa<LoadInst>(inst) or isa<StoreInst>(inst)) {
    //         auto memLoc = MemoryLocation::get(&inst);
    //         errs() << "memloc for inst " << inst << ": " << *memLoc.Ptr << "\n";
    //       }
    //     }
    //   }
    // }
    return false;

    instLogAnalysis.printInsts();

    // read in from log & do analysis on values
    /*
    Sample line:
      6 (instrID)
      0x7ffda4e5109c (addr)
      12 (instrID)
      0x7fa344e5112d (addr)
    */

    // get MemoryLocation from Instruction -> https://llvm.org/doxygen/classllvm_1_1MemoryLocation.html#a61dc6d1a1e9c3cb0adb4c791b329ff31

    // create actual InstLogAnalysis data structures
    return false;
  }
private:
  InstLogAnalysis instLogAnalysis;

}; // end of struct InstLogAnalysisWrapperPass
}  // end of anonymous namespace

char InstLogAnalysisWrapperPass::ID = 0;
static RegisterPass<InstLogAnalysisWrapperPass> X("fp_analysis", "InstLogAnalysisWrapperPass Pass",
                             false /* Only looks at CFG */,
                             false /* Analysis Pass */);

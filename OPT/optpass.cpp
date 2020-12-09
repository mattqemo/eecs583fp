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

#include "../ANALYSIS/analysis.cpp"

#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <fstream>
#include <utility>

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


loop:
  store(memLocB) (memLoc IS variant)
  %k = load(memLocA) (memLocA invariant)
  memLocA and memLocB never alias -> hoist load?
load(memLocA)\

common expression elimination

%memLocA = alloca i32 // I have storage somewhere for an i32 which could be a register


int main() {
  Class* a = new Class(argv[1]);
  Class* b = a;

  fn(a);
  fn(b);
}

common expression elimination?
double calcStuff(rect* first, rect* second) {
  return sqrt(first->area) / (sqrt(second->area) + 1);
}

arg1 = load f64 first
val1 = call sqrt(arg1)
arg2 = load f64 second
val2 = call sqrt(arg2)
val3 = add val2 1
val4 = div val1 val3
ret val4

...

arg1 = load f64 first
val1 = call sqrt(arg1)
val2 = val1
if (first != second) {
  arg2pre = load f64 second
  val2pre = call sqrt(arg2)
  val2 = val2pre -> store
}
arg2 = phi(arg1, arg2pre)
val2 = phi(val1, val2pre)
val3 = add val2 1
val4 = div val1 val3
ret val4


%i = load(memLocA)
%a = functionCall(%i) //
sqrt()
fib()
num_digits()
log()
toUpper()

void fcn(int og[], int copy[], int start, int end){
  memcpy(og, copy, len);
  if (og != copy) // do copy
}
int main(){
  fcn()
}

%j = load(memLocB)
%b = functionCall(%j)

%i = load(memLocA)
if (memLocA != memLocB) %k = load(memLocB)
%j = phi(%i, %k)


%hoisted = load(memLocA)
loop:
  %k1 = phi(%k2, %hoisted)
  store(memLocB)
  if (memLocB == memLocA) %i1 = load(memLocA)
  %k2 = phi(%i1, %k1)
  memLocA and memLocB never alias -> hoist load?

  for (i = 0 -> 100) {
    b[i] = x;
     a = b[CONSTANT] (not alias if CONSTANT > 100)
    a[i + 1] = y;
  }
*/


/*
PLAN:
  - pure function optimization
  - maybe also LICM (see if can disable register promotion)

Note: can examine compiled code with
https://stackoverflow.com/questions/10990018/how-to-generate-assembly-code-with-clang-in-intel-syntax
*/

namespace {

struct OptOnAliasProfilePass : public ModulePass {
  static char ID;

  void getAnalysisUsage(AnalysisUsage& AU) const {
    AU.addRequired<InstLogAnalysisWrapperPass>();
  }

  bool areFunctionCallsIdentical(const InstLogAnalysis& instLogAnalysis, CallBase* call1, CallBase* call2){
    assert(call1->getCalledFunction() == call2->getCalledFunction());
    assert(call1->arg_size() == call2->arg_size());

    for (auto it1 = call1->arg_begin(), it2 = call2->arg_begin(); it1 != call1->arg_end(); ++it1, ++it2) {
      // if (arg is Ptr) {
      //   memLoc from ptr
      //   getProba(ptr1, ptr2)
      // }
      auto* use1 = *it1, use2 = *it2;
      if (dyn_cast<MemoryLocation>(use1)) { // test this idea
        errs() << "casting worked\n";
        auto* memLoc1 = dyn_cast<MemoryLocation>(use1);
        auto* memLoc2 = dyn_cast<MemoryLocation>(use2);
        double probAlias = instLogAnalysis.getAliasProbability(memLoc1, memLoc2);
      }

      else if (use1->get() != use2->get()) { //compare their Value*
        // (use1->getUser() != use2->getUser())
        return false; //??
      }



    }

    return false;
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
  bool handleFunction(const InstLogAnalysis& instLogAnalysis, Function* f) {
    if (!f->getName().contains("_PURE_")) return false;
    std::unordered_map<Function*, std::vector<CallBase*>> prevFunctionCalls;
    for (auto& bb : f) {
      for (auto& inst : bb) {
        if (auto* currCall = dyn_cast<CallBase>(inst)) {
          auto* fCalled = currCall->getCalledFunction();
          if (prevFunctionCalls.count(fCalled)){
            // check for probable aliasing that meets our threshold
            auto& prevCallsVec = prevFunctionCalls[fCalled];
            for (auto* prevCall : prevCallsVec) {
              if (areFunctionCallsIdentical(instLogAnalysis, currCall, prevCall)) {
                // do smth idkkkk
                 // cleanup code for if we're wrong
              }
            }

          }
        }
      }
    }
  }

  bool runOnModule(Module &m) override {
    bool changed = false;

    auto& instLogAnalysis = getAnalysis<InstLogAnalysisWrapperPass>().getInstLogAnalysis();

    for (auto& f : m) {
      changed |= handleFunction(instLogAnalysis, f);
    }

    return changed;
  }
}; // end of struct OptOnAliasProfilePass
}  // end of anonymous namespace

char OptOnAliasProfilePass::ID = 0;
static RegisterPass<OptOnAliasProfilePass> X("fp_optimization", "OptOnAliasProfilePass Pass",
                             false /* Only looks at CFG */,
                             false /* Analysis Pass */);


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

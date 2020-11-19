#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Support/Format.h"
#include "llvm/Analysis/BranchProbabilityInfo.h"
#include "llvm/Support/BranchProbability.h"
#include <string>
using namespace llvm;

struct profiling: public FunctionPass {

static char ID;
profiling() : FunctionPass(ID) {}
void getAnalysisUsage(AnalysisUsage &AU) const {
        AU.addRequired<BlockFrequencyInfoWrapperPass>(); // Analysis pass to load block execution count
        AU.addRequired<BranchProbabilityInfoWrapperPass>(); // Analysis pass to load branch probability
}

bool runOnFunction(Function &F) override {
    return true;
};

char profiling::ID = 0;
static RegisterPass<mypass> X("profiling", "Benchmark Pass",
                             false /* Only looks at CFG */,
                                     true  /* Analysis Pass */);


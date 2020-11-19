#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Format.h"
#include "llvm/Analysis/BlockFrequencyInfo.h"
#include "llvm/Analysis/BranchProbabilityInfo.h"
#include "llvm/IR/Instructions.h"

#include "instructionSets.h"

#include <unordered_map>
#include <vector>

using namespace llvm;

namespace {
struct InjectInstLog : public ModulePass {
  static char ID;

  InjectInstLog() : ModulePass(ID) {}

/*
; Function Attrs: cold noinline nounwind optnone uwtable
define dso_local void @temp(i8* %ptr) #0 !prof !29 {
entry:
  %ptr.addr = alloca i8*, align 8
  store i8* %ptr, i8** %ptr.addr, align 8
  %0 = load i8*, i8** %ptr.addr, align 8
  %call = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([17 x i8], [17 x i8]* @.str, i64 0, i64 0), i8* %0)
  ret void
}
*/

  bool runOnModule(Module &m) override {
    bool changed = false;
    auto* instLogFunc = m.getFunction("temp");
    for (auto& func : m) {
      for (auto& bb : func) {
        for (auto& inst : bb) {
          if (isa<LoadInst>(inst) or isa<StoreInst>(inst)) {
            changed = true;
            errs() << "creating callinst\n";
            CallInst::Create(instLogFunc->getFunctionType(), instLogFunc, {}, "", &inst); //https://llvm.org/doxygen/classllvm_1_1CallInst.html
          }
        }
      }
    }
    return changed;
  }

}; // end of struct InjectInstLog
}  // end of anonymous namespace

char InjectInstLog::ID = 0;
static RegisterPass<InjectInstLog> X("fp", "InjectInstLog Pass",
                             false /* Only looks at CFG */,
                             false /* Analysis Pass */);

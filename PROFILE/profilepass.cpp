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

#include "helpers.hpp"

using namespace llvm;

namespace {
struct InjectInstLog : public ModulePass {
  static char ID;
  Function* instLogFunc = nullptr;
  Function* mainFunc = nullptr;

  InjectInstLog() : ModulePass(ID) {}

  void injectInstLogAfter(Instruction* inst, size_t instId, Value* ptrVal) {
    auto* IDParam = ConstantInt::get(
      instLogFunc->getFunctionType()->getFunctionParamType(0),
      instId);
    auto* castPtrParam = CastInst::CreatePointerCast(
      ptrVal,
      instLogFunc->getFunctionType()->getFunctionParamType(1),
      ""); // cast all pointers to whatever the instLogFunc accepts
    auto* instLogCall = CallInst::Create(instLogFunc->getFunctionType(), instLogFunc, {IDParam, castPtrParam}, "");
    castPtrParam->insertAfter(inst);
    instLogCall->insertAfter(castPtrParam);
  }

  bool runOnModule(Module &m) override {
    instLogFunc = m.getFunction("_inst_log");
    mainFunc = m.getFunction("main");
    assert(instLogFunc && "instLogFunc not found");
    assert(mainFunc && "mainFunc not found");
    auto* dataLayout = new DataLayout(&m);
    bool changed = false;
    auto ptrsToLog = getMemLocPtrToId(m);
    for (auto& func : m) {
      if (&func == instLogFunc) continue;
      for (auto& bb : func) {
        for (auto& inst : bb) {
          if (ptrsToLog.count(&inst)) {
            injectInstLogAfter(&inst, ptrsToLog[&inst], &inst);
            changed = true;
            ptrsToLog.erase(&inst);
          }
        }
      }
    }
    if (!ptrsToLog.empty()) {
      for (auto& [ptr, id] : ptrsToLog) {
        if (auto* inst = dyn_cast<Instruction>(ptr); inst && inst->getFunction() == instLogFunc) continue;
        changed = true;
        injectInstLogAfter(&mainFunc->getEntryBlock().front(), id, ptr);
      }
      ptrsToLog.clear();
    }
    return changed;
  }

}; // end of struct InjectInstLog
}  // end of anonymous namespace

char InjectInstLog::ID = 0;
static RegisterPass<InjectInstLog> X("fp_profile", "InjectInstLog Pass",
                             false /* Only looks at CFG */,
                             false /* Analysis Pass */);

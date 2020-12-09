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

/* TODO: replace CallInst with CallBase so exceptions can be thrown */

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
    auto ptrsToLog = getMemLocToId(m);
    for (auto& func : m) {
      for (auto& bb : func) {
        for (auto& inst : bb) {
          if (auto memLocOpt = MemoryLocation::getOrNone(&inst); memLocOpt.hasValue() && ptrsToLog.count(memLocOpt.getValue())) {
            // TODO: atone for const_cast sins
            auto* memLocPtr = const_cast<Value*>(memLocOpt.getValue().Ptr);
            auto memLocId = ptrsToLog[memLocOpt.getValue()];
            ptrsToLog.erase(memLocOpt.getValue());
            if (auto* memLocInst = dyn_cast<Instruction>(memLocPtr)) {
              if (memLocInst->getFunction() == instLogFunc) continue; // Do not inject logging into instlogfunc - unnecessary and will cause infinite recursion
              injectInstLogAfter(memLocInst, memLocId, memLocPtr);
            } else {
              injectInstLogAfter(&mainFunc->getEntryBlock().front(), memLocId, memLocPtr);
            }
            changed = true;
          }
        }
      }
    }
    assert(ptrsToLog.empty() && "Did not inject logging for every memory location!");
    return changed;
  }

}; // end of struct InjectInstLog
}  // end of anonymous namespace

char InjectInstLog::ID = 0;
static RegisterPass<InjectInstLog> X("fp_profile", "InjectInstLog Pass",
                             false /* Only looks at CFG */,
                             false /* Analysis Pass */);

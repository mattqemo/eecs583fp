#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Format.h"
#include "llvm/Analysis/BlockFrequencyInfo.h"
#include "llvm/Analysis/BranchProbabilityInfo.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/DataLayout.h"

#include "instructionSets.h"

#include <unordered_map>
#include <vector>
#include <type_traits>

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

  // template <typename MEM_INST_TYPE>
  // static uint64_t memInstSize(const MEM_INST_TYPE* memInst, const DataLayout* dataLayout) {
  //   static_assert(std::is_same_v<MEM_INST_TYPE, LoadInst> || std::is_same_v<MEM_INST_TYPE, StoreInst>, "Mem instructions must be one of LoadInst or StoreInst");
  //   return dataLayout->getTypeStoreSize(memInst->getPointerOperand()->getType()->getPointerElementType());
  //   // if (const auto* loadInst = dyn_cast<LoadInst>(memInst)) {
  //   //   errs() << "Load inst: " << *loadInst << "\n";
  //   //   errs() << "ptr operand" << *loadInst->getPointerOperand() << "\n";
  //   //   errs() << "type of pointer" << *loadInst->getPointerOperand()->getType() << "\n";
  //   //   errs() << "pointee element type" << *loadInst->getPointerOperand()->getType()->getPointerElementType() << "\n";
  //   //   return dataLayout->getTypeStoreSize(loadInst->getPointerOperand()->getType()->getPointerElementType());
  //   // } else if (const auto* storeInst = dyn_cast<StoreInst>(memInst)) {
  //   //   return dataLayout->getTypeStoreSize(storeInst->getPointerOperand()->getType()->getPointerElementType());
  //   // }
  // }



  static Value* getPointerValueOfMemInst(Instruction* memInst) {
    // static_assert(std::is_same_v<MEM_INST_TYPE, LoadInst> || std::is_same_v<MEM_INST_TYPE, StoreInst>, "Mem instructions must be one of LoadInst or StoreInst");
    if (auto* loadInst = dyn_cast<LoadInst>(memInst)) {
      return loadInst->getPointerOperand();
    } else if (auto* storeInst = dyn_cast<StoreInst>(memInst)) {
      return storeInst->getPointerOperand();
    } else {
      assert(false && "getPointerValueOfMemInst called on an instruction which was not a loadInst or a storeInst");
    }
  }

  bool runOnModule(Module &m) override {
    auto* dataLayout = new DataLayout(&m);
    bool changed = false;
    // auto& loopInfo = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
    auto* instLogFunc = m.getFunction("temp");
    for (auto& func : m) {
      if (&func == instLogFunc) continue;
      auto funcName = func.getName();
      for (auto& bb : func) {
        for (auto& inst : bb) {
          if (isa<LoadInst>(inst) or isa<StoreInst>(inst)) {
            changed = true;
            auto instName = inst.getName();
            errs() << "creating callinst\n";
            // TODO: Create constant value C-strings to pass to CallInst::Create
            // uint64_t size = memInstSize(&inst, dataLayout); // TODO: Create constant value and pass to func
            // TODO: Inject bitcast so arbitrary ptr value can be passed as void*
            // TODO: Get a create func from https://llvm.org/doxygen/classllvm_1_1CastInst.html
            // CallInst::Create(instLogFunc->getFunctionType(), instLogFunc, {getPointerValueOfMemInst(&inst)}, "", &inst); //https://llvm.org/doxygen/classllvm_1_1CallInst.html
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

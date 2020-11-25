#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Format.h"
#include "llvm/Analysis/BlockFrequencyInfo.h"
#include "llvm/Analysis/BranchProbabilityInfo.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Constants.h"
#include <array>

#include "instructionSets.h"

#include <unordered_map>
#include <vector>
#include <type_traits>

using namespace llvm;

namespace {
struct InjectInstLog : public ModulePass {
  static char ID;
  static const size_t INST_LOG_PTR_PARAM_INDEX = 0;
  static const size_t INST_LOG_SIZE_PARAM_INDEX = 1;

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
  static uint64_t memInstSize(const Instruction* memInst, const DataLayout* dataLayout) {
    if (const auto* loadInst = dyn_cast<LoadInst>(memInst)) {
      return dataLayout->getTypeStoreSize(loadInst->getPointerOperand()->getType()->getPointerElementType());
    } else if (const auto* storeInst = dyn_cast<StoreInst>(memInst)) {
      return dataLayout->getTypeStoreSize(storeInst->getPointerOperand()->getType()->getPointerElementType());
    }
  }



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
      auto* zeroValue = ConstantInt::get(Type::getInt64Ty(func.getContext()), 0);
      std::array<Value*, 2> gepIdxArgs = {zeroValue, zeroValue};
      for (auto& bb : func) {
        // https://stackoverflow.com/questions/16656855/llvm-ir-string-initialization
        auto* funcNameCStrParam = ConstantDataArray::getString(func.getContext(), func.getName());
        auto* funcNameCStrParamType = cast<ConstantDataArray>(funcNameCStrParam);

        // TODO: try https://llvm.org/doxygen/Instructions_8h_source.html#l00965
        // TODO: Pass correct params to gep create call - how to pass in pointer to our constant array funcNameCStrParam?
        auto* getEltPtrToFuncName = GetElementPtrInst::Create(PointerType::getUnqual(funcNameCStrParam->getType()), funcNameCStrParam, ArrayRef<Value*>(gepIdxArgs));
        for (auto& inst : bb) {
          if (isa<LoadInst>(inst) or isa<StoreInst>(inst)) {
            changed = true;
            errs() << "creating callinst\n";
            auto* instLogSizeParamType = instLogFunc->getFunctionType()->getFunctionParamType(INST_LOG_SIZE_PARAM_INDEX);
            auto* sizeParam = ConstantInt::get(instLogSizeParamType, memInstSize(&inst, dataLayout));
            auto* instLogPtrParamType = instLogFunc->getFunctionType()->getFunctionParamType(INST_LOG_PTR_PARAM_INDEX);
            auto* castPtrParam = CastInst::CreatePointerCast(getPointerValueOfMemInst(&inst), instLogPtrParamType, "", &inst); //https://llvm.org/doxygen/classllvm_1_1CastInst.html  

            // auto* instNameCStrParam = ConstantDataArray::getString(func.getContext(), inst.getName());
            // auto* getEltPtrToInstName = GetElementPtrInst::Create
            
            // TODO: add instName and funcName to Create call 
            CallInst::Create(instLogFunc->getFunctionType(), instLogFunc, {castPtrParam, sizeParam, funcNameCStrParam}, "", &inst); //https://llvm.org/doxygen/classllvm_1_1CallInst.html
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

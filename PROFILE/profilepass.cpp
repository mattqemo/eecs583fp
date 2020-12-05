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

#include <unordered_map>
#include <vector>
#include <type_traits>

using namespace llvm;

namespace {
struct InjectInstLog : public ModulePass {
  static char ID;

  InjectInstLog() : ModulePass(ID) {}

  static uint64_t memInstSize(const Instruction* memInst, const DataLayout* dataLayout) {
    if (const auto* loadInst = dyn_cast<LoadInst>(memInst)) {
      return dataLayout->getTypeStoreSize(loadInst->getPointerOperand()->getType()->getPointerElementType());
    } else if (const auto* storeInst = dyn_cast<StoreInst>(memInst)) {
      return dataLayout->getTypeStoreSize(storeInst->getPointerOperand()->getType()->getPointerElementType());
    }
  }

  static Value* getPointerValueOfMemInst(Instruction* memInst) {
    if (auto* loadInst = dyn_cast<LoadInst>(memInst)) {
      return loadInst->getPointerOperand();
    } else if (auto* storeInst = dyn_cast<StoreInst>(memInst)) {
      return storeInst->getPointerOperand();
    } else {
      assert(false && "getPointerValueOfMemInst called on an instruction which was not a loadInst or a storeInst");
    }
  }

  Constant* getElementPtrToCStr(Module& m, Function &func, const StringRef& string) {
    auto* zeroValue = ConstantInt::get(Type::getInt64Ty(func.getContext()), 0);
    std::array<Value*, 2> gepIdxArgs = {zeroValue, zeroValue};

    auto* strData = ConstantDataArray::getString(func.getContext(), string);
    // TODO: OK to just leak this, probably
    auto* gv = new GlobalVariable(/*Module=*/m,
      /*Type=*/strData->getType(),
      /*isConstant=*/true,
      /*Linkage=*/GlobalVariable::PrivateLinkage,
      /*Initializer=*/strData,
      /*Name=*/string);

    return ConstantExpr::getInBoundsGetElementPtr(strData->getType(), gv, gepIdxArgs);
  }

  llvm::DenseSet<Value*> getLoadedOrStoredPtrs(Module& m) {
    auto* instLogFunc = m.getFunction("_inst_log");
    llvm::DenseSet<Value*> ret;
    for (auto& func : m) {
      // if (&func == instLogFunc) continue;
      for (auto& bb : func) {
        for (auto& inst : bb) {
          if (auto* loadInst = dyn_cast<LoadInst>(&inst); loadInst != nullptr) {
            ret.insert(loadInst->getPointerOperand());
            // errs() << "memory location of type " << cast<Instruction>(loadInst->getPointerOperand())->getOpcodeName() << "\n";
            // auto memLoc = MemoryLocation::get(loadInst);
            // errs() << "memloc for inst " << inst << ": " << *memLoc.Ptr << "\n";
          } else if (auto* storeInst = dyn_cast<StoreInst>(&inst); storeInst != nullptr) {
            ret.insert(storeInst->getPointerOperand());
            // errs() << "memory location of type " << cast<Instruction>(storeInst->getPointerOperand())->getOpcodeName() << "\n";
            // auto memLoc = MemoryLocation::get(storeInst);
            // errs() << "memloc for inst " << inst << ": " << *memLoc.Ptr << "\n";
          }
        }
      }
    }
    return ret;
  }

  /*
    TODO: Log pointer value after each MEMORY LOCATION initialization
    MEMORY LOCATION - pointer used by load/store
    Should be mostly allocas (memory element), some loads (pointer to pointer situation) and some GEP (registers), etc.
    TODO: figure out how to log global pointers as well

    TODO: emit data as
    instid
    addr
  */
  bool runOnModule(Module &m) override {
    auto* dataLayout = new DataLayout(&m);
    bool changed = false;
    auto* instLogFunc = m.getFunction("_inst_log");
    auto* mainFunc = m.getFunction("main");
    assert(instLogFunc && "instLogFunc not found");
    assert(mainFunc && "mainFunc not found");
    auto instsToLog = getLoadedOrStoredPtrs(m);
    // errs() << *instLogFunc << "\n";
    size_t instID = 0;
    for (auto& func : m) {
      if (&func == instLogFunc) continue;
      // auto* funcNameStrPtr = getElementPtrToCStr(m, func, func.getName());
      for (auto& bb : func) {
        for (auto& inst : bb) {
          if (instsToLog.count(&inst)) {
            errs() << "memory location of type " << inst.getOpcodeName() << "\n";
            if (inst.getOpcode() != Instruction::Alloca &&
                inst.getOpcode() != Instruction::Load &&
                inst.getOpcode() != Instruction::GetElementPtr) {
                  errs() << "memory location of type " << inst.getOpcodeName() << "\n";
                }
            size_t currentInstID = instID++;
            auto* IDParam = ConstantInt::get(
              instLogFunc->getFunctionType()->getFunctionParamType(0),
              currentInstID);
            auto* castPtrParam = CastInst::CreatePointerCast(
              &inst,
              instLogFunc->getFunctionType()->getFunctionParamType(1),
              ""); //https://llvm.org/doxygen/classllvm_1_1CastInst.html
            auto* instLogCall = CallInst::Create(instLogFunc->getFunctionType(), instLogFunc, {IDParam, castPtrParam}, ""); //https://llvm.org/doxygen/classllvm_1_1CallInst.html
            castPtrParam->insertAfter(&inst);
            instLogCall->insertAfter(castPtrParam);
            changed = true;
            instsToLog.erase(&inst);
          }
          // if (isa<LoadInst>(inst) or isa<StoreInst>(inst)) {
          //   size_t currentInstID = instID++;
          //   char memInstType = isa<LoadInst>(inst) ? 'L' : 'S';
          //   changed = true;
          //   auto* IDParam = ConstantInt::get(
          //     instLogFunc->getFunctionType()->getFunctionParamType(0),
          //     currentInstID);
          //   auto* castPtrParam = CastInst::CreatePointerCast(
          //     getPointerValueOfMemInst(&inst),
          //     instLogFunc->getFunctionType()->getFunctionParamType(1),
          //     "",
          //     &inst); //https://llvm.org/doxygen/classllvm_1_1CastInst.html
          //   auto* sizeParam = ConstantInt::get(
          //     instLogFunc->getFunctionType()->getFunctionParamType(2),
          //     memInstSize(&inst, dataLayout));
          //   auto* memTypeParam = ConstantInt::get(
          //     instLogFunc->getFunctionType()->getFunctionParamType(3),
          //     memInstType);

          //   CallInst::Create(instLogFunc->getFunctionType(), instLogFunc, {IDParam, castPtrParam, sizeParam, memTypeParam, funcNameStrPtr}, "", &inst); //https://llvm.org/doxygen/classllvm_1_1CallInst.html
          // }
        }
      }
    }
    if (!instsToLog.empty()) {
      errs() << "error: some insts which should have been logged were not instructions\n";
      for (auto* instToLog : instsToLog) {
        errs() << *instToLog << "\n";
      }
      assert(false && "error: some insts which should have been logged were not instructions\n");
    }
    return changed;
  }

}; // end of struct InjectInstLog
}  // end of anonymous namespace

char InjectInstLog::ID = 0;
static RegisterPass<InjectInstLog> X("fp_profile", "InjectInstLog Pass",
                             false /* Only looks at CFG */,
                             false /* Analysis Pass */);

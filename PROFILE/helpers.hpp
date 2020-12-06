#ifndef _HELPERS_H_
#define _HELPERS_H_

#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

// Return a map where the keys are every pointer ever loaded/stored in the program,
// and the values are an id assigned by the order in which the pointers are referenced
llvm::DenseMap<Value*, size_t> getMemLocPtrToId(Module& m) {
  llvm::DenseMap<Value*, size_t> ret;
  size_t id = 0;
  for (auto& func : m) {
    for (auto& bb : func) {
      for (auto& inst : bb) {
        if (auto* loadInst = dyn_cast<LoadInst>(&inst)) {
          if (ret.find(loadInst->getPointerOperand()) == ret.end()) ret[loadInst->getPointerOperand()] = id++;
        } else if (auto* storeInst = dyn_cast<StoreInst>(&inst)) {
          if (ret.find(storeInst->getPointerOperand()) == ret.end()) ret[storeInst->getPointerOperand()] = id++;
        }
      }
    }
  }
  return ret;
}

#endif /* _HELPERS_H_ */

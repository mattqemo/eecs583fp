#ifndef _HELPERS_H_
#define _HELPERS_H_

#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include <unordered_map>
#include <unordered_set>

using namespace llvm;

template <>
struct std::hash<MemoryLocation> {
  using hash_type_t = std::hash<const llvm::Value*>;
  std::size_t operator()(const MemoryLocation& memLoc) const noexcept { return hash_type_t{}(memLoc.Ptr); }
};

// Return a map where the keys are every pointer ever loaded/stored in the program,
// and the values are an id assigned by the order in which the pointers are referenced
std::unordered_map<MemoryLocation, size_t> getMemLocToId(Module& m) {
  auto ret = std::unordered_map<MemoryLocation, size_t>{};
  auto allocaMemLocs = std::unordered_set<MemoryLocation>{};

  size_t id = 0;
  for (auto& func : m) {
    for (auto& bb : func) {
      for (auto& inst : bb) {
        if (auto memLocOpt = MemoryLocation::getOrNone(&inst); memLocOpt.hasValue()) {
          if (!ret.count(memLocOpt.getValue())) {
            ret[memLocOpt.getValue()] = id++;
            allocaMemLocs.insert(memLocOpt.getValue());
          }
          // adds loads of pointers (we check they load pointers with the 2nd condition)
          if (isa<LoadInst>(&inst) && inst.getType()->isPtrOrPtrVectorTy() && allocaMemLocs.count(memLocOpt.getValue())) {
            ret[MemoryLocation(&inst)] = id++; // this memoryLocation construction is jank
          }
        }
      }
    }
  }
  return ret;
}

#endif /* _HELPERS_H_ */

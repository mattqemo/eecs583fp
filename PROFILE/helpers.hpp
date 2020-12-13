#ifndef _HELPERS_H_
#define _HELPERS_H_

#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include <unordered_map>

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
  size_t id = 0;
  for (auto& func : m) {
    for (auto& bb : func) {
      for (auto& inst : bb) {
        if (auto memLocOpt = MemoryLocation::getOrNone(&inst); memLocOpt.hasValue() && !ret.count(memLocOpt.getValue())) {
          // if (isa<LoadInst>(inst)) {
          //   errs() << inst << '\n';
          // }
          // errs() << "memLoc added with value of " << *memLocOpt.getValue().Ptr << ' ' << inst << "\n";
          ret[memLocOpt.getValue()] = id++;
        }
      }
    }
  }
  return ret;
}

#endif /* _HELPERS_H_ */

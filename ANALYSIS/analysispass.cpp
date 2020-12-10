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

#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <fstream>
#include <utility>

#include "../PROFILE/helpers.hpp"

using namespace llvm;

/*
TODO: address potential issue that our profile data might be invalidated by other transforming passes
running BEFORE our last pass.
TODO: Allow running this pass with requireAnalysis<> or something (whatever it's called)
*/

namespace fp583 {
typedef std::pair<MemoryLocation, MemoryLocation> MemLocPair;

struct hashMemLocPair {
  size_t operator()(const MemLocPair& p) const {
    auto hash1 = std::hash<void*>{}((void*)p.first.Ptr);
    auto hash2 = std::hash<void*>{}((void*)p.second.Ptr);
    return hash1 ^ hash2;
  }
};

struct AliasStats {
  uint32_t num_collisions;
  uint32_t num_comparisons;

  AliasStats() : num_collisions(0), num_comparisons(0) {}
};
struct InstLogAnalysis {
  std::unordered_map<MemLocPair, AliasStats, hashMemLocPair> memLocPairToAliasStats;

  double getAliasProbability(const MemoryLocation& loc_a, const MemoryLocation& loc_b) const {
    if (loc_a.Ptr == loc_b.Ptr) {
      return 1.0;
    }

    auto it = InstLogAnalysis::memLocPairToAliasStats.find({loc_a, loc_b});
    if (it == InstLogAnalysis::memLocPairToAliasStats.end()) {
      return 0.0;
    }
    return (double)it->second.num_collisions / it->second.num_comparisons;
  }
};

struct InstLogAnalysisWrapperPass : public ModulePass {
  static char ID;
  std::unordered_map<size_t, MemoryLocation> idToMemLoc;

  InstLogAnalysisWrapperPass() : ModulePass(ID) {}

  std::unordered_map<size_t, MemoryLocation> getIdToMemLocMapping(Module &m) const {
    std::unordered_map<MemoryLocation, size_t> memLocToId = getMemLocToId(m);
    std::unordered_map<size_t, MemoryLocation> idToMemLoc;
    for (auto& [memLoc, Id] : memLocToId) {
      idToMemLoc[Id] = memLoc;
    }
    return idToMemLoc;
  }

  std::unordered_map<MemLocPair, AliasStats, hashMemLocPair> parseLogAndGetAliasStats() const {
    std::unordered_map<size_t, uint64_t> idToShadowValue;
    std::unordered_map<MemLocPair, AliasStats, hashMemLocPair> memLocPairToAliasStats;

    size_t instIdIn;
    void* memAddrIn_void; // TODO: change to uint64_t directly?
    std::ifstream ins("../583simple/log.log");
    while (ins >> instIdIn >> memAddrIn_void) {
      auto memLocIn = idToMemLoc.at(instIdIn);
      uint64_t memAddrIn = (uint64_t)memAddrIn_void;
      idToShadowValue[instIdIn] = memAddrIn;

      for (auto it_shadow = idToShadowValue.begin(); it_shadow != idToShadowValue.end(); ++it_shadow) {
        auto memLocCompare = idToMemLoc.at(it_shadow->first);
        uint64_t memAddrCompare = it_shadow->second;

        if (memLocCompare.Ptr != memLocIn.Ptr) { // don't compute aliasing stats with itself
          auto& pairAliasStats = memLocPairToAliasStats[{memLocIn, memLocCompare}];
          pairAliasStats.num_comparisons++;
          if (memAddrIn == memAddrCompare) {
            pairAliasStats.num_collisions++;
            // errs() << "\tCOLLISION DETECTED\n";
          }
        }
      }
    }

    return memLocPairToAliasStats;
  }

  void testGetAliasProba(Module& m, size_t targetId_a, size_t targetId_b) {
    MemoryLocation memLoc_a, memLoc_b;
    std::unordered_set<const Value*> visitedPtrs;
    size_t currId = 0;

    for (auto& func : m) {
      for (auto& bb : func) {
        for (auto& inst : bb) {
          if (isa<LoadInst>(inst) or isa<StoreInst>(inst)) {
            auto memLoc = MemoryLocation::get(&inst);
            if (!visitedPtrs.count(memLoc.Ptr)) {
              if (currId == targetId_a) memLoc_a = memLoc;
              if (currId == targetId_b) memLoc_b = memLoc;
              ++currId;
              visitedPtrs.insert(memLoc.Ptr);
            }
          }
        }
      }
    }


    double aliasProba = instLogAnalysis.getAliasProbability(memLoc_a, memLoc_b);
    errs() << "AliasProba between ID " << targetId_a << " and ID " << targetId_b << " is " << aliasProba << '\n';
  }

  bool runOnModule(Module &m) override {
    // TODO: use morgans function and flip
    idToMemLoc = getIdToMemLocMapping(m);

    std::unordered_map<MemLocPair, AliasStats, hashMemLocPair> memLocPairToAliasStats = parseLogAndGetAliasStats();

    instLogAnalysis.memLocPairToAliasStats = memLocPairToAliasStats;

    // testGetAliasProba(m, 2, 5);
    // testGetAliasProba(m, 12, 8);
    // testGetAliasProba(m, 1, 1);
    return false;
  }

  InstLogAnalysis& getInstLogAnalysis() { return instLogAnalysis; }

private:
  InstLogAnalysis instLogAnalysis;

}; // end of struct InstLogAnalysisWrapperPass
}  // end of anonymous namespace

char fp583::InstLogAnalysisWrapperPass::ID = 0;
static RegisterPass<fp583::InstLogAnalysisWrapperPass> X("fp_analysis", "InstLogAnalysisWrapperPass Pass",
                             false /* Only looks at CFG */,
                             false /* Analysis Pass */);



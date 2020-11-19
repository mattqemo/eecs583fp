#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Format.h"
#include "llvm/Analysis/BlockFrequencyInfo.h"
#include "llvm/Analysis/BranchProbabilityInfo.h"

#include "instructionSets.h"

#include <unordered_map>
#include <vector>

using namespace llvm;

namespace {
enum InstructionsType {
  Int_ALU, 
  Float_ALU, 
  Mem, 
  BiasedBranch,
  UnbiasedBranch,
  Others,
  Count 
};

struct FunctionStats {
  std::vector<uint64_t> instructions_count;
  uint64_t dynamic_ops_count;

  FunctionStats(): dynamic_ops_count(0), instructions_count(std::vector<uint64_t>(InstructionsType::Count, 0)) {}
};

struct ComputeStats : public FunctionPass {
  static char ID;

  ComputeStats() : FunctionPass(ID) {}

  void getAnalysisUsage(AnalysisUsage& AU) const {
    AU.addRequired<BlockFrequencyInfoWrapperPass>(); // get BB exe count
    AU.addRequired<BranchProbabilityInfoWrapperPass>(); // get branch proba
  }

  InstructionsType getInstructionType(const BranchProbabilityInfo &bpi, const BasicBlock& basicBlock, const Instruction& inst) {
    if (InstructionSets::isInt_ALU(inst)) return Int_ALU;
    if (InstructionSets::isFloat_ALU(inst)) return Float_ALU;
    if (InstructionSets::isMem(inst)) return Mem;
    if (InstructionSets::isBranch(inst)) {
      return bpi.getHotSucc(&basicBlock) ? BiasedBranch : UnbiasedBranch; 
    }
    return Others;
  }

  float getCorrespondingRatio(const FunctionStats& Fstats, size_t instType) {
    if (Fstats.dynamic_ops_count == 0) {
      return 0.0f;
    }
    return float(Fstats.instructions_count[instType]) / Fstats.dynamic_ops_count;
  }

  void outputFunctionStats(const Function& F, const FunctionStats& Fstats) {
    errs() << F.getName();
    errs() << ", " << Fstats.dynamic_ops_count;
    for (size_t instType = 0; instType < Fstats.instructions_count.size(); ++instType) {
      errs() << ", " << format("%f", getCorrespondingRatio(Fstats, instType));
    }
    errs() << "\n";
  }

  bool runOnFunction(Function &F) override {
    BlockFrequencyInfo &bfi = getAnalysis<BlockFrequencyInfoWrapperPass>().getBFI();
    BranchProbabilityInfo &bpi = getAnalysis<BranchProbabilityInfoWrapperPass>().getBPI(); 
    FunctionStats Fstats;

    for (const BasicBlock& basicBlock : F.getBasicBlockList()) {
      llvm::Optional<uint64_t> block_count = bfi.getBlockProfileCount(&basicBlock);
      if (block_count.hasValue()) {
        Fstats.dynamic_ops_count += block_count.getValue() * basicBlock.getInstList().size();

        for (const Instruction& inst: basicBlock.getInstList()) {
          InstructionsType instType = getInstructionType(bpi, basicBlock, inst);
          Fstats.instructions_count[instType] += block_count.getValue();
        }   
      }   
    }

    outputFunctionStats(F, Fstats);

    return false;
  }

}; // end of struct ComputeStats
}  // end of anonymous namespace

char ComputeStats::ID = 0;
static RegisterPass<ComputeStats> X("hw1", "ComputeStats Pass",
                             false /* Only looks at CFG */,
                             false /* Analysis Pass */);

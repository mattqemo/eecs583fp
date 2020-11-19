#include "llvm/IR/Instruction.h"

#include <unordered_set>

using namespace llvm;

struct InstructionSets {
  static const std::unordered_set<unsigned> Int_ALU, Float_ALU, Mem, Branch;

  static bool isInt_ALU(const Instruction& inst) {
    return Int_ALU.find(inst.getOpcode()) != Int_ALU.end();
  }

  static bool isFloat_ALU(const Instruction& inst) {
    return Float_ALU.find(inst.getOpcode()) != Float_ALU.end();
  }

  static bool isMem(const Instruction& inst) {
    return Mem.find(inst.getOpcode()) != Mem.end();
  }

  static bool isBranch(const Instruction& inst) {
    return Branch.find(inst.getOpcode()) != Branch.end();
  }
};

const std::unordered_set<unsigned> InstructionSets::Int_ALU = {
  Instruction::Add, 
  Instruction::Sub, 
  Instruction::Mul, 
  Instruction::UDiv, 
  Instruction::SDiv, 
  Instruction::URem, 
  Instruction::Shl,
  Instruction::LShr,
  Instruction::AShr,
  Instruction::And,
  Instruction::Or,
  Instruction::Xor,
  Instruction::ICmp,
  Instruction::SRem
};

const std::unordered_set<unsigned> InstructionSets::Float_ALU = {
  Instruction::FAdd,
  Instruction::FSub,
  Instruction::FMul,
  Instruction::FDiv,
  Instruction::FRem,
  Instruction::FCmp
};

const std::unordered_set<unsigned> InstructionSets::Mem = {
  Instruction::Alloca,
  Instruction::Load,
  Instruction::Store,
  Instruction::GetElementPtr,
  Instruction::Fence,
  Instruction::AtomicCmpXchg,
  Instruction::AtomicRMW
};

const std::unordered_set<unsigned> InstructionSets::Branch = {
  Instruction::Br,
  Instruction::Switch,
  Instruction::IndirectBr,
};
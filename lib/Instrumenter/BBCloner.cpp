#include "llvm/Constants.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Instructions.h"
#include "llvm/Module.h"
#include "llvm/Pass.h"

#include "rcs/IDAssigner.h"

using namespace llvm;
using namespace rcs;

namespace loom {
struct BBCloner: public FunctionPass {
  static char ID;

  BBCloner(): FunctionPass(ID) {}
  virtual void getAnalysisUsage(AnalysisUsage &AU) const;
  virtual bool doInitialization(Module &M);
  virtual bool runOnFunction(Function &F);

 private:
  // scalar types
  Type *VoidType, *IntType;
  Function *Slot; // slot function
};
}

using namespace loom;

char BBCloner::ID = 0;

static RegisterPass<BBCloner> X("clone-bbs",
                                "Clone basic blocks",
                                false,
                                false);

void BBCloner::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<IDAssigner>();
}

bool BBCloner::doInitialization(Module &M) {
  VoidType = Type::getVoidTy(M.getContext());
  IntType = Type::getInt32Ty(M.getContext());

  FunctionType *SlotType = FunctionType::get(VoidType, IntType, false);
  Slot = Function::Create(SlotType,
                          GlobalValue::ExternalLinkage,
                          "LoomSlot",
                          &M);
  return true;
}

bool BBCloner::runOnFunction(Function &F) {
  IDAssigner &IDA = getAnalysis<IDAssigner>();
  for (Function::iterator B = F.begin(); B != F.end(); ++B) {
    for (BasicBlock::iterator I = B->begin(); I != B->end(); ++I) {
      unsigned InsID = IDA.getInstructionID(I);
      if (InsID != IDAssigner::InvalidID) {
        // in the original program
        CallInst::Create(Slot, ConstantInt::get(IntType, InsID), "", I);
      }
    }
  }
  return false;
}

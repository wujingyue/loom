#include "llvm/Constants.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Instructions.h"
#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/CallSite.h"

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
        // <I> exists in the original program.
        // We want to keep LoomBeforeBlocking and LoomAfterBlocking tightly
        // around a blocking callsite, LoomEnterThread right before
        // pthread_create, and LoomExitThread right after pthread_join.
        // Threfore, be super careful about the insertion position of LoomSlot.
        // e.g., after inserting LoomSlot to the following code snippet
        //   call LoomBeforeBlocking
        //   call read
        //   call LoomAfterBlocking
        //   ret void
        // the code should look like:
        //   call LoomSlot
        //   call LoomBeforeBlocking
        //   call read
        //   call LoomAfterBlocking
        //   call LoomSlot
        //   ret void
        BasicBlock::iterator InsertPos = I;
        while (InsertPos != B->begin()) {
          BasicBlock::iterator Prev = InsertPos; --Prev;
          CallSite CS(Prev);
          if (!CS)
            break;
          Function *Callee = CS.getCalledFunction();
          if (!Callee)
            break;
          if (Callee->getName() != "LoomBeforeBlocking" &&
              Callee->getName() != "LoomEnterThread")
            break;
          InsertPos = Prev;
        }
        CallInst::Create(Slot, ConstantInt::get(IntType, InsID), "", InsertPos);
      }
    }
  }
  return false;
}

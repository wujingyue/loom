#include "llvm/Instructions.h"
#include "llvm/Module.h"
#include "llvm/Pass.h"

#include "rcs/IDAssigner.h"

using namespace llvm;
using namespace rcs;

namespace loom {
struct BreakCriticalInvokes: public FunctionPass {
  static char ID;

  BreakCriticalInvokes(): FunctionPass(ID) {}
  virtual void getAnalysisUsage(AnalysisUsage &AU) const;
  virtual bool runOnFunction(Function &F);
};
}

using namespace loom;

char BreakCriticalInvokes::ID = 0;

static RegisterPass<BreakCriticalInvokes> X(
    "break-crit-invokes",
    "Break critical edges from InvokeInsts",
    true, // is CFG only
    false); // is analysis

void BreakCriticalInvokes::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<IDAssigner>();
  AU.addPreserved<IDAssigner>();
}

bool BreakCriticalInvokes::runOnFunction(Function &F) {
  for (Function::iterator B = F.begin(); B != F.end(); ++B) {
    if (InvokeInst *II = dyn_cast<InvokeInst>(B->getTerminator())) {
      assert(II->getNormalDest() != II->getUnwindDest());
      BasicBlock *NormalDest = II->getNormalDest();
      if (NormalDest->getUniquePredecessor() == NULL) {
        // <II>'s normal destination has multiple predecessors. Therefore, the
        // edge from <B> to <NormalDest> is critical, and we need to break it.
        BasicBlock *Critical = BasicBlock::Create(F.getContext(), "crit", &F);
        BranchInst::Create(NormalDest, Critical);
        for (BasicBlock::iterator I = NormalDest->begin();
             PHINode *Phi = dyn_cast<PHINode>(I);
             ++I) {
          for (unsigned j = 0; j < Phi->getNumIncomingValues(); ++j) {
            if (Phi->getIncomingBlock(j) == B)
              Phi->setIncomingBlock(j, Critical);
          }
        }
        II->setNormalDest(Critical);
      }
    }
  }
  return true;
}

#include <vector>

#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/CallSite.h"

#include "rcs/IDAssigner.h"
#include "rcs/IdentifyBackEdges.h"
#include "rcs/IdentifyThreadFuncs.h"

#include "loom/IdentifyBlockingCS.h"

using namespace std;
using namespace llvm;
using namespace rcs;
using namespace loom;

namespace loom {
struct CheckInserter: public FunctionPass {
  static char ID;

  CheckInserter(): FunctionPass(ID) {}
  virtual void getAnalysisUsage(AnalysisUsage &AU) const;
  virtual bool doInitialization(Module &M);
  virtual bool runOnFunction(Function &F);

 private:
  static void InsertAfter(Instruction *I, Instruction *Pos);

  void checkFeatures(Function &F);
  void insertCycleChecks(Function &F);
  void insertBlockingChecks(Function &F);
  void insertThreadChecks(Function &F);

  // scalar types
  Type *VoidType, *IntType;

  // checks
  Function *CycleCheck;
  Function *BeforeBlocking, *AfterBlocking;
  Function *EnterThread;
};
}

char CheckInserter::ID = 0;

static RegisterPass<CheckInserter> X("insert-checks",
                                     "Insert loom checks",
                                     false,
                                     false);

// TODO: Looks general to put into rcs.
void CheckInserter::InsertAfter(Instruction *I, Instruction *Pos) {
  if (TerminatorInst *TI = dyn_cast<TerminatorInst>(Pos)) {
    for (size_t j = 0; j < TI->getNumSuccessors(); ++j) {
      Instruction *I2 = (j == 0 ? I : I->clone());
      I2->insertBefore(TI->getSuccessor(j)->getFirstInsertionPt());
    }
  } else {
    I->insertAfter(Pos);
  }
}

void CheckInserter::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<IdentifyThreadFuncs>();
  AU.addRequired<IdentifyBackEdges>();
  AU.addRequired<IdentifyBlockingCS>();
}

bool CheckInserter::doInitialization(Module &M) {
  // setup scalar types
  VoidType = Type::getVoidTy(M.getContext());
  IntType = Type::getInt32Ty(M.getContext());

  // setup checks
  // These checks share the same function type fortunately.
  FunctionType *CheckType = FunctionType::get(VoidType, IntType, false);
  // setup loom_cycle_check
  CycleCheck = Function::Create(CheckType,
                                GlobalValue::ExternalLinkage,
                                "LoomCycleCheck",
                                &M);
  // setup loom_before/after_blocking
  BeforeBlocking = Function::Create(CheckType,
                                    GlobalValue::ExternalLinkage,
                                    "LoomBeforeBlocking",
                                    &M);
  AfterBlocking = Function::Create(CheckType,
                                   GlobalValue::ExternalLinkage,
                                   "LoomAfterBlocking",
                                   &M);
  // setup loom_enter/exit_thread
  EnterThread = Function::Create(CheckType,
                                 GlobalValue::ExternalLinkage,
                                 "LoomEnterThread",
                                 &M);

  // Return true because we added new function declarations.
  return true;
}

// Check the assumptions we made.
void CheckInserter::checkFeatures(Function &F) {
  // InvokeInst's unwind destination has only one predecessor.
  for (Function::iterator B = F.begin(); B != F.end(); ++B) {
    for (BasicBlock::iterator I = B->begin(); I != B->end(); ++I) {
      if (InvokeInst *II = dyn_cast<InvokeInst>(I)) {
        assert(II->getUnwindDest()->getUniquePredecessor() != NULL);
      }
    }
  }
}

bool CheckInserter::runOnFunction(Function &F) {
  IdentifyThreadFuncs &IDF = getAnalysis<IdentifyThreadFuncs>();

  checkFeatures(F);

  insertCycleChecks(F);
  insertBlockingChecks(F);
  if (IDF.isThreadFunction(F)) {
    insertThreadChecks(F);
  }

  return true;
}

void CheckInserter::insertCycleChecks(Function &F) {
  IdentifyBackEdges &IBE = getAnalysis<IdentifyBackEdges>();

  for (Function::iterator B1 = F.begin(); B1 != F.end(); ++B1) {
    TerminatorInst *TI = B1->getTerminator();
    for (unsigned j = 0; j < TI->getNumSuccessors(); ++j) {
      BasicBlock *B2 = TI->getSuccessor(j);
      unsigned BackEdgeID = IBE.getID(B1, B2);
      if (BackEdgeID != (unsigned)-1) {
        BasicBlock *BackEdgeBlock = BasicBlock::Create(
            F.getContext(),
            "backedge_" + B1->getName() + "_" + B2->getName(),
            &F);
        CallInst::Create(CycleCheck,
                         ConstantInt::get(IntType, BackEdgeID),
                         "",
                         BackEdgeBlock);
        // BackEdgeBlock -> B2
        BranchInst::Create(B2, BackEdgeBlock);
        // B1 -> BackEdgeBlock
        // There might be multiple back edges from B1 to B2. Need to replace
        // them all.
        for (unsigned j2 = j; j2 < TI->getNumSuccessors(); ++j2) {
          if (TI->getSuccessor(j2) == B2) {
            TI->setSuccessor(j2, BackEdgeBlock);
          }
        }
      }
    }
  }
}

void CheckInserter::insertBlockingChecks(Function &F) {
  IdentifyBlockingCS &IBCS = getAnalysis<IdentifyBlockingCS>();

  for (Function::iterator B = F.begin(); B != F.end(); ++B) {
    for (BasicBlock::iterator I = B->begin(); I != B->end(); ++I) {
      unsigned CallSiteID = IBCS.getID(I);
      if (CallSiteID != (unsigned)-1) {
        CallInst::Create(BeforeBlocking,
                         ConstantInt::get(IntType, CallSiteID),
                         "",
                         I);
        CallInst *CallAfterBlocking = CallInst::Create(
            AfterBlocking,
            ConstantInt::get(IntType, CallSiteID));
        InsertAfter(CallAfterBlocking, I);
      }
    }
  }
}

void CheckInserter::insertThreadChecks(Function &F) {
  CallInst::Create(EnterThread,
                   ConstantInt::get(IntType, -1), // invalid id
                   "",
                   F.begin()->getFirstInsertionPt());
  // LoomEnterThread registers LoomExitThread as a cleanup routine. Therefore,
  // we needn't insert call to LoomExitThread here.
}

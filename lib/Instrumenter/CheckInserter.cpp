#include <vector>

#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/CallSite.h"
#include "llvm/Support/raw_ostream.h"

#include "rcs/IDAssigner.h"
#include "rcs/IdentifyBackEdges.h"

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
  virtual bool doFinalization(Module &M);

 private:
  static void InsertAfter(Instruction *I, Instruction *Pos);

  void checkFeatures(Module &M);
  void insertCycleChecks(Function &F);
  void insertBlockingChecks(Function &F);
  void instrumentThread(Function &F);

  // scalar types
  Type *VoidType, *IntType;
  FunctionType *InitFiniType;

  // checks
  Function *CycleCheck;
  Function *BeforeBlocking, *AfterBlocking;
  Function *EnterThread, *ExitThread;
  Function *EnterProcess;
};
}

char CheckInserter::ID = 0;

static RegisterPass<CheckInserter> X("insert-checks",
                                     "Insert loom checks",
                                     false,
                                     false);

void CheckInserter::InsertAfter(Instruction *I, Instruction *Pos) {
  if (InvokeInst *II = dyn_cast<InvokeInst>(Pos)) {
    // FIXME: we should instrument the unwind BB as well, but it looks hard. The
    // unwind BB has to begin with a landingpad. Also, we need to break the edge
    // from the invoke to its unwind BB if the edge is critical. For now, we
    // only instrument the normal destination.
    // We assume the edge from the invoke to its normal destination is not
    // critical, because we run BreakCriticalInvokes beforehand.
    assert(II->getNormalDest()->getUniquePredecessor() != NULL);
    I->insertBefore(II->getNormalDest()->getFirstInsertionPt());
  } else {
    I->insertAfter(Pos);
  }
}

void CheckInserter::getAnalysisUsage(AnalysisUsage &AU) const {
  // make sure IDAssigner is run before CheckInserter
  AU.addRequired<IDAssigner>();
  AU.addRequired<IdentifyBackEdges>();
  AU.addRequired<IdentifyBlockingCS>();
  AU.addPreserved<IDAssigner>();
}

bool CheckInserter::doInitialization(Module &M) {
  checkFeatures(M);

  // setup scalar types
  VoidType = Type::getVoidTy(M.getContext());
  IntType = Type::getInt32Ty(M.getContext());

  // setup checks
  FunctionType *CheckType = FunctionType::get(VoidType, IntType, false);
  InitFiniType = FunctionType::get(VoidType, false);

  CycleCheck = Function::Create(CheckType,
                                GlobalValue::ExternalLinkage,
                                "LoomCycleCheck",
                                &M);

  BeforeBlocking = Function::Create(CheckType,
                                    GlobalValue::ExternalLinkage,
                                    "LoomBeforeBlocking",
                                    &M);
  AfterBlocking = Function::Create(CheckType,
                                   GlobalValue::ExternalLinkage,
                                   "LoomAfterBlocking",
                                   &M);

  EnterThread = Function::Create(InitFiniType,
                                 GlobalValue::ExternalLinkage,
                                 "LoomEnterThread",
                                 &M);
  ExitThread = Function::Create(InitFiniType,
                                GlobalValue::ExternalLinkage,
                                "LoomExitThread",
                                &M);

  EnterProcess = Function::Create(InitFiniType,
                                  GlobalValue::ExternalLinkage,
                                  "LoomEnterProcess",
                                  &M);

  // Return true because we added new function declarations.
  return true;
}

// Check the assumptions we made.
void CheckInserter::checkFeatures(Module &M) {
  // Assume no function name starts with Loom.
  for (Module::iterator F = M.begin(); F != M.end(); ++F) {
    assert(!F->getName().startswith("Loom") &&
           "Loom update engine seems already instrumented");
  }
  // We do not support the situation where some important functions are called
  // via a function pointer, e.g. pthread_create, pthread_join and fork.
  for (Module::iterator F = M.begin(); F != M.end(); ++F) {
    if (F->getName() == "pthread_create" ||
        F->getName() == "pthread_join" ||
        F->getName() == "fork") {
      for (Value::use_iterator UI = F->use_begin(); UI != F->use_end(); ++UI) {
        User *Usr = *UI;
        assert(isa<CallInst>(Usr) || isa<InvokeInst>(Usr));
        CallSite CS(cast<Instruction>(Usr));
        for (unsigned i = 0; i < CS.arg_size(); ++i)
          assert(CS.getArgument(i) != F);
      }
    }
  }
}

bool CheckInserter::runOnFunction(Function &F) {
  insertCycleChecks(F);
  insertBlockingChecks(F);
  instrumentThread(F);
  return true;
}

bool CheckInserter::doFinalization(Module &M) {
  // We couldn't directly add an element to a constant array, because doing so
  // changes the type of the constant array.

  // element type of llvm.global_ctors
  StructType *ST = StructType::get(IntType,
                                   PointerType::getUnqual(InitFiniType),
                                   NULL); // end with null

  // Move all existing elements of <GlobalName> to <Constants>.
  vector<Constant *> Constants;
  if (GlobalVariable *GlobalCtors = M.getNamedGlobal("llvm.global_ctors")) {
    ConstantArray *CA = cast<ConstantArray>(GlobalCtors->getInitializer());
    for (unsigned j = 0; j < CA->getNumOperands(); ++j) {
      ConstantStruct *CS = cast<ConstantStruct>(CA->getOperand(j));
      assert(CS->getType() == ST);
      // Assume nobody is using the highest priority, so that <F> will be the
      // first to run as a ctor.
      assert(!cast<ConstantInt>(CS->getOperand(0))->isMinValue(true));
      Constants.push_back(CS);
    }
    GlobalCtors->eraseFromParent();
  }
  // Add <F> with the highest priority.
  Constants.push_back(ConstantStruct::get(ST,
                                          ConstantInt::get(IntType, INT_MIN),
                                          EnterProcess,
                                          NULL));
  // Create the new llvm.global_ctors.
  ArrayType *ArrType = ArrayType::get(ST, Constants.size());
  new GlobalVariable(M,
                     ArrType,
                     true,
                     GlobalValue::AppendingLinkage,
                     ConstantArray::get(ArrType, Constants),
                     "llvm.global_ctors");

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
        // Fix the PHINodes in B2.
        BranchInst::Create(B2, BackEdgeBlock);
        for (BasicBlock::iterator I = B2->begin();
             B2->getFirstNonPHI() != I;
             ++I) {
          PHINode *PHI = cast<PHINode>(I);
          // Note: If B2 has multiple incoming edges from B1 (e.g. B1 terminates
          // with a SelectInst), its PHINodes must also have multiple incoming
          // edges from B1. However, after adding BackEdgeBlock and essentially
          // merging the multiple incoming edges from B1, there will be only one
          // edge from BackEdgeBlock to B2. Therefore, we need to remove the
          // redundant incoming edges from B2's PHINodes.
          bool FirstIncomingFromB1 = true;
          for (unsigned k = 0; k < PHI->getNumIncomingValues(); ++k) {
            if (PHI->getIncomingBlock(k) == B1) {
              if (FirstIncomingFromB1) {
                FirstIncomingFromB1 = false;
                PHI->setIncomingBlock(k, BackEdgeBlock);
              } else {
                PHI->removeIncomingValue(k, false);
                --k;
              }
            }
          }
        }
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

void CheckInserter::instrumentThread(Function &F) {
  // FIXME: we assume pthread_create and pthread_join always succeed for now.
  for (Function::iterator B = F.begin(); B != F.end(); ++B) {
    for (BasicBlock::iterator I = B->begin(); I != B->end(); ++I) {
      CallSite CS(I);
      if (CS) {
        if (Function *Callee = CS.getCalledFunction()) {
          if (Callee->getName() == "pthread_create") {
            CallInst::Create(EnterThread, "", I);
          }
          if (Callee->getName() == "pthread_join") {
            InsertAfter(CallInst::Create(ExitThread), I);
          }
        }
      }
    }
  }
}

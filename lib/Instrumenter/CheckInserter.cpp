#include <vector>

#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/CallSite.h"
#include "llvm/Support/raw_ostream.h"

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
  virtual bool doFinalization(Module &M);

 private:
  static void InsertAfter(Instruction *I, Instruction *Pos);

  void checkFeatures(Module &M);
  void checkFeatures(Function &F);

  void insertCycleChecks(Function &F);
  void insertBlockingChecks(Function &F);
  void insertThreadChecks(Function &F);
  bool addCtorOrDtor(Module &M, Function &F, const string &GlobalName);

  // scalar types
  Type *VoidType, *IntType;
  FunctionType *IniterType;

  // checks
  Function *CycleCheck;
  Function *BeforeBlocking, *AfterBlocking;
  Function *EnterThread, *ExitThread;
  Function *EnterProcess, *ExitProcess;
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
  checkFeatures(M);

  // setup scalar types
  VoidType = Type::getVoidTy(M.getContext());
  IntType = Type::getInt32Ty(M.getContext());

  // setup checks
  FunctionType *CheckType = FunctionType::get(VoidType, IntType, false);
  IniterType = FunctionType::get(VoidType, false);

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

  EnterThread = Function::Create(IniterType,
                                 GlobalValue::ExternalLinkage,
                                 "LoomEnterThread",
                                 &M);
  ExitThread = Function::Create(IniterType,
                                GlobalValue::ExternalLinkage,
                                "LoomExitThread",
                                &M);

  EnterProcess = Function::Create(IniterType,
                                  GlobalValue::ExternalLinkage,
                                  "LoomEnterProcess",
                                  &M);
  ExitProcess = Function::Create(IniterType,
                                 GlobalValue::ExternalLinkage,
                                 "LoomExitProcess",
                                 &M);

  // Return true because we added new function declarations.
  return true;
}

// Check the assumptions we made.
void CheckInserter::checkFeatures(Module &M) {
  // We do not support the situation where some important functions are called
  // via a function pointer, e.g. pthread_exit, pthread_create, and fork.
  for (Module::iterator F = M.begin(); F != M.end(); ++F) {
    if (F->getName() == "pthread_exit" ||
        F->getName() == "pthread_create" ||
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

  // We do not support the situation where a thread function is called
  // directly without using pthread_create.
  IdentifyThreadFuncs &IDF = getAnalysis<IdentifyThreadFuncs>();
  if (IDF.isThreadFunction(F)) {
    for (Value::use_iterator UI = F.use_begin(); UI != F.use_end(); ++UI) {
      CallSite CS(*UI);
      assert(CS);
      Function *Callee = CS.getCalledFunction();
      assert(Callee && Callee->getName() == "pthread_create");
      // Assume <F> is the first argument of pthread_create.
      assert(CS.arg_size() > 0 && CS.getArgument(0) == &F);
      for (unsigned i = 1; i < CS.arg_size(); ++i)
        assert(CS.getArgument(i) != &F);
    }
  }
}

bool CheckInserter::runOnFunction(Function &F) {
  checkFeatures(F);

  insertCycleChecks(F);
  insertBlockingChecks(F);
  // Run insertThreadChecks after the other two.
  insertThreadChecks(F);

  return true;
}

bool CheckInserter::doFinalization(Module &M) {
  bool Result = false;
  Result |= addCtorOrDtor(M, *EnterProcess, "llvm.global_ctors");
  Result |= addCtorOrDtor(M, *ExitProcess, "llvm.global_dtors");
  return Result;
}

bool CheckInserter::addCtorOrDtor(Module &M,
                                  Function &F,
                                  const string &GlobalName) {
  // We couldn't directly add an element to a constant array, because doing so
  // changes the type of the constant array.

  // element type of llvm.global_ctors/llvm.global_dtors
  StructType *ST = StructType::get(IntType,
                                   PointerType::getUnqual(IniterType),
                                   NULL); // end with null

  // Move all existing elements of <GlobalName> to <Constants>.
  vector<Constant *> Constants;
  if (GlobalVariable *GlobalCtors = M.getNamedGlobal(GlobalName)) {
    ConstantArray *CA = cast<ConstantArray>(GlobalCtors->getInitializer());
    for (unsigned j = 0; j < CA->getNumOperands(); ++j) {
      ConstantStruct *CS = cast<ConstantStruct>(CA->getOperand(j));
      assert(CS->getType() == ST);
      // Assume nobody is using the highest priority, so that <F> will be the
      // first (last) to run as a ctor (dtor).
      assert(!cast<ConstantInt>(CS->getOperand(0))->isMinValue(true));
      Constants.push_back(CS);
    }
    GlobalCtors->eraseFromParent();
  }
  // Add <F> with the highest priority.
  Constants.push_back(ConstantStruct::get(ST,
                                          ConstantInt::get(IntType, INT_MIN),
                                          &F,
                                          NULL));
  // Create the new <GlobalName> (llvm.global_ctors or llvm.global_dtors).
  ArrayType *ArrType = ArrayType::get(ST, Constants.size());
  new GlobalVariable(M,
                     ArrType,
                     true,
                     GlobalValue::AppendingLinkage,
                     ConstantArray::get(ArrType, Constants),
                     GlobalName);

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

void CheckInserter::insertThreadChecks(Function &F) {
  IdentifyThreadFuncs &IDF = getAnalysis<IdentifyThreadFuncs>();

  // Add LoomExitThread before each pthread_exit().
  for (Function::iterator B = F.begin(); B != F.end(); ++B) {
    for (BasicBlock::iterator I = B->begin(); I != B->end(); ++I) {
      CallSite CS(I);
      if (CS) {
        Function *Callee = CS.getCalledFunction();
        if (Callee && Callee->getName() == "pthread_exit") {
          CallInst::Create(ExitThread, "", I);
        }
      }
    }
  }

  // If <F> is a thread function, add EnterThread at its entry, and add
  // ExitThread at its exits.
  if (IDF.isThreadFunction(F)) {
    CallInst::Create(EnterThread, "", F.begin()->getFirstInsertionPt());
    for (Function::iterator B = F.begin(); B != F.end(); ++B) {
      TerminatorInst *TI = B->getTerminator();
      if (isa<ReturnInst>(TI)) {
        CallInst::Create(ExitThread, "", B->getTerminator());
      }
    }
  }
}

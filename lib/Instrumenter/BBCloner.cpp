#include "llvm/Constants.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Instructions.h"
#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/Dominators.h"
#include "llvm/Support/CallSite.h"
#include "llvm/Support/CFG.h"
#include "llvm/Support/IRBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/SSAUpdater.h"

#include "rcs/IDAssigner.h"
#include "rcs/typedefs.h"

#include "loom/config.h"

using namespace std;
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
  static bool IsBackEdgeBlock(const BasicBlock &B);

  void CloneBBs(Function &F);
  void CreateFastPath(Function &F);
  void InsertSwitches(Function &F);
  void UpdateSSA(Function &F);
  void InsertSlots(Function &F);
  void InsertSlots(BasicBlock &B);
  void verifyLoomSlots(BasicBlock &B);

  // scalar types
  Type *VoidType, *IntType;
  Function *Slot, *Switch;
  ValueToValueMapTy CloneMap;
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
  AU.addRequired<DominatorTree>();
}

bool BBCloner::doInitialization(Module &M) {
  VoidType = Type::getVoidTy(M.getContext());
  IntType = Type::getInt32Ty(M.getContext());

  FunctionType *SlotType = FunctionType::get(VoidType, IntType, false);
  Slot = Function::Create(SlotType,
                          GlobalValue::ExternalLinkage,
                          "LoomSlot",
                          &M);

  FunctionType *SwitchType = FunctionType::get(IntType, IntType, false);
  Switch = Function::Create(SwitchType,
                            GlobalValue::ExternalLinkage,
                            "LoomSwitch",
                            &M);

  return true;
}

bool BBCloner::runOnFunction(Function &F) {
  CloneBBs(F);
  InsertSlots(F);
  return true;
}

bool BBCloner::IsBackEdgeBlock(const BasicBlock &B) {
  for (BasicBlock::const_iterator I = B.begin(); I != B.end(); ++I) {
    ImmutableCallSite CS(I);
    if (CS) {
      if (const Function *Callee = CS.getCalledFunction()) {
        if (Callee->getName() == "LoomCycleCheck")
          return true;
      }
    }
  }
  return false;
}

void BBCloner::CreateFastPath(Function &F) {
  CloneMap.clear();

  for (Function::arg_iterator AI = F.arg_begin(); AI != F.arg_end(); ++AI)
    CloneMap[AI] = AI;

  BBSet OldBBs;
  for (Function::iterator B = F.begin(); B != F.end(); ++B)
    OldBBs.insert(B);

  for (BBSet::iterator I = OldBBs.begin(); I != OldBBs.end(); ++I) {
    BasicBlock *B = *I;
    // Skip the back edge blocks inserted by CheckInserter.
    if (IsBackEdgeBlock(*B)) {
      CloneMap[B] = B;
      continue;
    }
    BasicBlock *B2 = CloneBasicBlock(B, CloneMap, ".fast", &F, NULL);
    // Strip DebugLoc from all cloned instructions; otherwise, the code
    // generator would assert fail. TODO: Figure out why it would fail.
    for (BasicBlock::iterator Ins = B2->begin(); Ins != B2->end(); ++Ins) {
      if (!Ins->getDebugLoc().isUnknown())
        Ins->setDebugLoc(DebugLoc());
    }
    CloneMap[B] = B2;
  }

  for (Function::iterator B2 = F.begin(); B2 != F.end(); ++B2) {
    if (OldBBs.count(B2))
      continue;
    for (BasicBlock::iterator I = B2->begin(); I != B2->end(); ++I)
      RemapInstruction(I, CloneMap);
  }
}

void BBCloner::InsertSwitches(Function &F) {
  // Insert LoomSwitch at each back edge.
  IDAssigner &IDA = getAnalysis<IDAssigner>();
  unsigned FuncID = IDA.getFunctionID(&F);
  assert(FuncID < MaxNumFuncs);
  for (Function::iterator B = F.begin(); B != F.end(); ++B) {
    if (IsBackEdgeBlock(*B)) {
      BasicBlock *OldTarget = *succ_begin(B);
      BasicBlock *NewTarget = cast<BasicBlock>(CloneMap.lookup(OldTarget));
      B->getTerminator()->eraseFromParent();
      IRBuilder<> Builder(B);
      Value *Slow = Builder.CreateCall(Switch,
                                       ConstantInt::get(IntType, FuncID));
      Builder.CreateCondBr(Builder.CreateIsNotNull(Slow), OldTarget, NewTarget);
    }
  }

  // Insert LoomSwitch at the function entry.
  {
    BasicBlock *OldEntry = F.begin();
    BasicBlock *NewEntry = cast<BasicBlock>(CloneMap.lookup(OldEntry));
    BasicBlock *Entry = BasicBlock::Create(F.getContext(),
                                           "entry.loom",
                                           &F,
                                           OldEntry);
    IRBuilder<> Builder(Entry);
    Value *Slow = Builder.CreateCall(Switch, ConstantInt::get(IntType, FuncID));
    Builder.CreateCondBr(Builder.CreateIsNotNull(Slow), OldEntry, NewEntry);
  }
}

void BBCloner::UpdateSSA(Function &F) {
  DominatorTree &DT = getAnalysis<DominatorTree>();
  // The function has been greatly modified since the beginning.
  DT.runOnFunction(F);

  vector<pair<Instruction *, Use *> > ToResolve;
  for (ValueToValueMapTy::iterator I = CloneMap.begin();
       I != CloneMap.end();
       ++I) {
    Value *Key = const_cast<Value *>(I->first);
    if (Instruction *OldIns = dyn_cast<Instruction>(Key)) {
      for (Value::use_iterator UI = OldIns->use_begin();
           UI != OldIns->use_end();
           ++UI) {
        if (Instruction *User = dyn_cast<Instruction>(*UI)) {
          if (!DT.dominates(OldIns, User))
            ToResolve.push_back(make_pair(OldIns, &UI.getUse()));
        }
      }
      Instruction *NewIns = cast<Instruction>(I->second);
      for (Value::use_iterator UI = NewIns->use_begin();
           UI != NewIns->use_end();
           ++UI) {
        if (Instruction *User = dyn_cast<Instruction>(*UI)) {
          if (!DT.dominates(NewIns, User)) {
            // Use OldIns intentionally.
            ToResolve.push_back(make_pair(OldIns, &UI.getUse()));
          }
        }
      }
    }
  }

  for (size_t i = 0; i < ToResolve.size(); ) {
    Instruction *OldIns = ToResolve[i].first;
    Instruction *NewIns = cast<Instruction>(CloneMap.lookup(OldIns));
    SSAUpdater SU;
    SU.Initialize(OldIns->getType(), OldIns->getName());
    SU.AddAvailableValue(OldIns->getParent(), OldIns);
    SU.AddAvailableValue(NewIns->getParent(), NewIns);
    size_t j = i;
    while (j < ToResolve.size() && ToResolve[j].first == ToResolve[i].first) {
      SU.RewriteUse(*ToResolve[j].second);
      ++j;
    }
    i = j;
  }
}

void BBCloner::CloneBBs(Function &F) {
  CreateFastPath(F);
  InsertSwitches(F);
  UpdateSSA(F);
}

void BBCloner::InsertSlots(BasicBlock &B) {
  IDAssigner &IDA = getAnalysis<IDAssigner>();
  // PHINodes and landingpad should be groupted at top of BB. We use
  // <Insertable> to indicate whether <I> already passes the first insertion
  // position. If so, insert LoomSlot before <I>; otherwise insert LoomSlot at
  // the first insertion position.
  Instruction *FirstInsertPos = B.getFirstInsertionPt();
  bool Insertable = false;
  for (BasicBlock::iterator I = B.begin(); I != B.end(); ++I) {
    if (FirstInsertPos == I)
      Insertable = true;
    unsigned InsID = IDA.getInstructionID(I);
    if (InsID != IDAssigner::InvalidID) {
      assert(InsID < MaxNumInsts);
      // <I> exists in the original program.
      BasicBlock::iterator InsertPos;
      if (!Insertable) {
        InsertPos = FirstInsertPos;
      } else {
        // We want to keep LoomBeforeBlocking and LoomAfterBlocking tightly
        // around a blocking callsite, LoomEnterThread right at the thread
        // entry, and LoomExitThread right before the thread exit.  Threfore, be
        // super careful about the insertion position of LoomSlot.  e.g., after
        // inserting LoomSlot to the following code snippet
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
        InsertPos = I;
        while (InsertPos != B.begin()) {
          BasicBlock::iterator Prev = InsertPos; --Prev;
          CallSite CS(Prev);
          if (!CS)
            break;
          Function *Callee = CS.getCalledFunction();
          if (!Callee)
            break;
          if (Callee->getName() != "LoomBeforeBlocking" &&
              Callee->getName() != "LoomExitThread")
            break;
          InsertPos = Prev;
        }
      }
      CallInst::Create(Slot, ConstantInt::get(IntType, InsID), "", InsertPos);
    }
  }

  // Verify LoomSlots are in a correct order.
  verifyLoomSlots(B);
}

void BBCloner::InsertSlots(Function &F) {
  for (Function::iterator B = F.begin(); B != F.end(); ++B) {
    // Only add LoomSlots in old BBs.
    if (CloneMap.count(B))
      InsertSlots(*B);
  }
}

void BBCloner::verifyLoomSlots(BasicBlock &B) {
  unsigned Last = -1;
  for (BasicBlock::iterator I = B.begin(); I != B.end(); ++I) {
    CallSite CS(I);
    if (CS && CS.getCalledFunction() == Slot) {
      assert(CS.arg_size() == 1);
      unsigned SlotID = cast<ConstantInt>(CS.getArgument(0))->getZExtValue();
      assert(Last == (unsigned)-1 || Last + 1 == SlotID);
      Last = SlotID;
    }
  }
}

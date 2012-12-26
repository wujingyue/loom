#include <fstream>
#include <string>

#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include "rcs/IDAssigner.h"
#include "rcs/typedefs.h"

using namespace std;
using namespace llvm;
using namespace rcs;

namespace loom {
struct Compiler: public ModulePass {
  static char ID;

  Compiler(): ModulePass(ID) {}
  virtual void getAnalysisUsage(AnalysisUsage &AU) const;
  virtual bool runOnModule(Module &M);
  virtual void print(raw_ostream &O, const Module *M) const;

 private:
  bool Error; // indicate there is any error in compiling the .lm file
  int FilterType;
  InstList StartOps, EndOps;
  FuncSet FuncsToPatch;
};
}
using namespace loom;

static RegisterPass<Compiler> X(
    "compile",
    "Compile a Loom file down to a low-level execution filter",
    false,
    true);

static cl::opt<string> LoomFileName("lm", cl::desc("Loom file name"));

char Compiler::ID = 0;

void Compiler::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
  AU.addRequired<IDAssigner>();
}

bool Compiler::runOnModule(Module &M) {
  IDAssigner &IDA = getAnalysis<IDAssigner>();

  Error = false;

  ifstream LoomFile(LoomFileName.c_str());
  if (!LoomFile) {
    errs() << LoomFileName << " does not exist.\n";
    Error = true;
    return false;
  }

  unsigned NumOps;
  if (!(LoomFile >> FilterType >> NumOps)) {
    errs() << "wrong format\n";
    Error = true;
    return false;
  }

  FuncsToPatch.clear();
  for (unsigned i = 0; i < NumOps; ++i) {
    int StartEnd, SlotID;
    if (!(LoomFile >> StartEnd >> SlotID)) {
      errs() << "wrong format\n";
      Error = true;
      return false;
    }
    Instruction *I = IDA.getInstruction(SlotID);
    if (I == NULL) {
      errs() << "slot " << SlotID << " does not exist.\n";
      Error = true;
      return false;
    }
    (StartEnd ? EndOps : StartOps).push_back(I);
    FuncsToPatch.insert(I->getParent()->getParent());
  }

  return false;
}

void Compiler::print(raw_ostream &O, const Module *M) const {
  if (Error)
    return;

  IDAssigner &IDA = getAnalysis<IDAssigner>();

  O << FilterType << "\n\n";
  O << StartOps.size() + EndOps.size() << "\n";
  for (size_t i = 0; i < StartOps.size(); ++i)
    O << "0 " << IDA.getInstructionID(StartOps[i]) << "\n";
  for (size_t i = 0; i < EndOps.size(); ++i)
    O << "1 " << IDA.getInstructionID(EndOps[i]) << "\n";

  O << "\n" << FuncsToPatch.size() << "\n";
  for (FuncSet::const_iterator I = FuncsToPatch.begin();
       I != FuncsToPatch.end();
       ++I) {
    O << IDA.getFunctionID(*I) << "\n";
  }

  O << "\n0\n\n0\n";
}

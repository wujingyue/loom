#ifndef __LOOM_IDENTIFY_BLOCKING_CS_H
#define __LOOM_IDENTIFY_BLOCKING_CS_H

#include <vector>

#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/ADT/DenseMap.h"

using namespace llvm;

namespace loom {
struct IdentifyBlockingCS: public ModulePass {
  static char ID;

  static bool IsBlockingExternal(const Function &F);

  IdentifyBlockingCS(): ModulePass(ID) {}
  virtual bool runOnModule(Module &M);
  virtual void getAnalysisUsage(AnalysisUsage &AU) const;
  unsigned getID(const Instruction *I) const;

 private:
  DenseMap<const Instruction *, unsigned> CallSite2ID;
};
}

#endif

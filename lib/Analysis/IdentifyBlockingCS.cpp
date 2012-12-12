#define DEBUG_TYPE "loom"

#include <string>

#include "llvm/ADT/Statistic.h"
#include "llvm/Support/CallSite.h"

#include "loom/IdentifyBlockingCS.h"

using namespace std;
using namespace loom;

static RegisterPass<IdentifyBlockingCS> X(
    "identify-blocking-cs",
    "Identify blocking external call sites",
    false,
    true);

STATISTIC(NumBlockingCallSites, "Number of blocking external call sites");

char IdentifyBlockingCS::ID = 0;

bool IdentifyBlockingCS::IsBlockingExternal(const Function &F) {
  string Name = F.getName();
  return (Name == "connect" ||
          Name == "open" ||
          Name == "accept" ||
          Name == "fork" ||
          Name == "abort" ||
          Name == "select" ||
          Name == "fwrite" ||
          Name == "fprintf" ||
          Name == "write" ||
          Name == "printf" ||
          Name == "puts" ||
          Name == "fputc" ||
          Name == "fputs" ||
          Name == "sendto" ||
          Name == "read" ||
          Name == "recvfrom" ||
          Name == "pthread_join" ||
          Name == "pthread_mutex_lock" ||
          Name == "pthread_cond_wait" ||
          Name == "pthread_cond_timedwait");
}

void IdentifyBlockingCS::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
}

bool IdentifyBlockingCS::runOnModule(Module &M) {
  for (Module::iterator F = M.begin(); F != M.end(); ++F) {
    for (Function::iterator B = F->begin(); B != F->end(); ++B) {
      for (BasicBlock::iterator I = B->begin(); I != B->end(); ++I) {
        CallSite CS(I);
        if (CS) {
          // FIXME: we assume function pointers do not point to external blocking
          // functions, which is not always true.
          if (Function *Callee = CS.getCalledFunction()) {
            if (IsBlockingExternal(*Callee)) {
              unsigned CallSiteID = CallSite2ID.size();
              CallSite2ID[I] = CallSiteID;
            }
          }
        }
      }
    }
  }
  NumBlockingCallSites = CallSite2ID.size();
  return false;
}

unsigned IdentifyBlockingCS::getID(const Instruction *I) const {
  DenseMap<const Instruction *, unsigned>::const_iterator IT;
  IT = CallSite2ID.find(I);
  if (IT == CallSite2ID.end())
    return -1;
  return IT->second;
}

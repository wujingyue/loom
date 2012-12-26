#ifndef __LOOM_CTL_H
#define __LOOM_CTL_H

#include "llvm/Support/CommandLine.h"

using namespace llvm;

namespace loom {

enum CtlAction {
  server, add, del, ls, ps
};

int RunControllerServer();
int RunControllerClient(CtlAction ControllerAction,
                        const cl::list<std::string> &Args);
}

#endif

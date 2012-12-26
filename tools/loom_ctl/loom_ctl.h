#ifndef __LOOM_CTL_H
#define __LOOM_CTL_H

#include "llvm/Support/CommandLine.h"

using namespace llvm;

namespace loom {
int RunControllerServer();
int RunControllerClient(const cl::opt<std::string> &ControllerAction,
                        const cl::list<std::string> &Args);
}

#endif

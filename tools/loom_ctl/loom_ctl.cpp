#include <pthread.h>
#include <signal.h>
#include <arpa/inet.h>

#include <cstdio>
#include <sstream>
#include <vector>
#include <map>

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include "loom/config.h"
#include "loom/Utils.h"
#include "loom_ctl.h"

using namespace std;
using namespace llvm;
using namespace loom;

// TODO: find a way to display more description on action
static cl::opt<string> ControllerAction(cl::Positional,
                                        cl::desc("<action>"),
                                        cl::Required);
static cl::list<string> Args(cl::ConsumeAfter, cl::desc("<arguments>..."));

int main(int argc, char *argv[]) {
  cl::ParseCommandLineOptions(argc, argv, "Loom controller");
  if (ControllerAction == "server") {
    if (RunControllerServer() == -1)
      return 1;
  } else if (ControllerAction == "add" ||
             ControllerAction == "delete" ||
             ControllerAction == "ls" ||
             ControllerAction == "ps") {
    if (RunControllerClient(ControllerAction, Args) == -1)
      return 1;
  } else {
    errs() << "Not implemented yet\n";
  }
  return 0;
}

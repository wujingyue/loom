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

static cl::opt<CtlAction> ControllerAction(
    cl::desc("Choose action:"),
    cl::values(
        clEnumVal(server, "Run the Loom controller server"),
        clEnumVal(add, "Add an execution filter: -add <PID> <file>"),
        clEnumVal(del, "Delete an execution filter: -del <PID> <filter ID>"),
        clEnumVal(ls, "List all filters or filters on a process: -ls [PID]"),
        clEnumVal(ps, "List all daemon processes: -ps"),
        clEnumValEnd),
    cl::init(server));
static cl::list<string> Args(cl::Positional, cl::desc("<arguments>..."));

int main(int argc, char *argv[]) {
  cl::ParseCommandLineOptions(argc, argv, "Loom controller");

  if (ControllerAction == server) {
    if (!Args.empty()) {
      errs() << "wrong format\n";
      return 1;
    }
    if (RunControllerServer() == -1)
      return 1;
   return 0;
  }

  if (RunControllerClient(ControllerAction, Args) == -1)
    return 1;
  return 0;
}

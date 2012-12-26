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

static int CommandAddFilter(int CtrlServerSock, const string &FilterFileName) {
  ostringstream OS;
  OS << "add ";
  // Convert to full path whenever possible, so that the user can use relative
  // path.
  if (char *FullPath = realpath(FilterFileName.c_str(), NULL)) {
    OS << FullPath;
    free(FullPath);
  } else {
    OS << FilterFileName;
  }
  return SendMessage(CtrlServerSock, OS.str().c_str());
}

static int CommandDeleteFilter(int CtrlServerSock, unsigned FilterID) {
  ostringstream OS;
  OS << "del " << FilterID;
  return SendMessage(CtrlServerSock, OS.str().c_str());
}

static int CommandListFilters(int CtrlServerSock, pid_t PID = -1) {
  ostringstream OS;
  OS << "ls";
  if (PID != -1)
    OS << " " << PID;
  return SendMessage(CtrlServerSock, OS.str().c_str());
}

static int CommandListDaemons(int CtrlServerSock) {
  return SendMessage(CtrlServerSock, "ps");
}

int loom::RunControllerClient(CtlAction ControllerAction,
                              const cl::list<string> &Args) {
  // TODO: check format before connecting to the controller server
  int CtrlServerSock = socket(AF_INET, SOCK_STREAM, 0);
  if (CtrlServerSock == -1) {
    perror("socket");
    return -1;
  }

  struct sockaddr_in ServerAddr;
  bzero(&ServerAddr, sizeof ServerAddr);
  ServerAddr.sin_family = AF_INET;
  ServerAddr.sin_addr.s_addr = inet_addr(CONTROLLER_IP);
  ServerAddr.sin_port = htons(CONTROLLER_PORT);
  if (connect(CtrlServerSock,
              (struct sockaddr *)&ServerAddr, sizeof ServerAddr) == -1) {
    perror("failed to connect to the controller server");
    return -1;
  }

  if (SendMessage(CtrlServerSock, "iam loom_ctl") == -1) {
    close(CtrlServerSock);
    return -1;
  }

  // TODO: "goto" doesn't seem a good habit.
  switch (ControllerAction) {
    case add:
      if (Args.size() != 1)
        goto format_error;
      if (CommandAddFilter(CtrlServerSock, Args[0]) == -1)
        goto error;
      break;
    case del:
      if (Args.size() != 1)
        goto format_error;
      // TODO: check Args[0] is a number
      if (CommandDeleteFilter(CtrlServerSock, atoi(Args[0].c_str())) == -1)
        goto error;
      break;
    case ls:
      if (Args.size() >= 2)
        goto format_error;
      if (Args.size() == 1) {
        if (CommandListFilters(CtrlServerSock, atoi(Args[0].c_str())) == -1)
          goto error;
      } else {
        if (CommandListFilters(CtrlServerSock) == -1)
          goto error;
      }
      break;
    case ps:
      if (Args.size() != 0)
        goto format_error;
      if (CommandListDaemons(CtrlServerSock) == -1)
        goto error;
      break;
    default:
      assert(false);
  }

  {
    char Response[MaxBufferSize] = {'\0'};
    if (ReceiveMessage(CtrlServerSock, Response) == -1)
      goto error;
    outs() << Response << "\n";
  }

  close(CtrlServerSock);
  return 0;

format_error:
  errs() << "wrong format\n";
error:
  close(CtrlServerSock);
  return -1;
}

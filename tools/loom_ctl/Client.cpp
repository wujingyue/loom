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

int loom::RunControllerClient(const cl::opt<string> &ControllerAction,
                        const cl::list<string> &Args) {
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

  // TODO: extra it to a function
  int Ret = 0;
  if (ControllerAction == "add") {
    if (Args.size() != 1) {
      errs() << "wrong format\n";
      Ret = -1;
    } else {
      Ret = CommandAddFilter(CtrlServerSock, Args[0]);
    }
  } else if (ControllerAction == "delete") {
    if (Args.size() != 1) {
      errs() << "wrong format\n";
      Ret = -1;
    } else {
      // TODO: check Args[0] is a number
      Ret = CommandDeleteFilter(CtrlServerSock, atoi(Args[0].c_str()));
    }
  } else if (ControllerAction == "ls") {
    if (Args.size() >= 2) {
      errs() << "wrong format\n";
      Ret = -1;
    } else if (Args.size() == 1) {
      Ret = CommandListFilters(CtrlServerSock, atoi(Args[0].c_str()));
    } else {
      Ret = CommandListFilters(CtrlServerSock);
    }
  } else if (ControllerAction == "ps") {
    if (Args.size() != 0) {
      errs() << "wrong format\n";
      Ret = -1;
    } else {
      Ret = CommandListDaemons(CtrlServerSock);
    }
  } else {
    assert(false);
  }

  // Don't bother receiving the response if sending already failed.
  if (Ret == 0) {
    char Response[MaxBufferSize] = {'\0'};
    Ret = ReceiveMessage(CtrlServerSock, Response);
    if (Ret == 0) {
      outs() << Response << "\n";
    }
  }

  close(CtrlServerSock);
  return Ret;
}

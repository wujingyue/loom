#include <pthread.h>
#include <arpa/inet.h>

#include <cstdio>
#include <sstream>
#include <vector>

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include "loom/config.h"
#include "loom/Utils.h"

using namespace std;
using namespace llvm;

// TODO: find a way to display more description on action
static cl::opt<string> ControllerAction(cl::Positional,
                                        cl::desc("<action>"),
                                        cl::Required);
static cl::list<string> Args(cl::ConsumeAfter, cl::desc("<arguments>..."));

// protect <CtrlClientSock> and <DaemonSocks>
static pthread_mutex_t Mutex = PTHREAD_MUTEX_INITIALIZER;
static int CtrlClientSock = -1;
static vector<int> DaemonSocks;
// needn't be protected, because it is only used by HandleControllerClient.
static vector<string> FilterFileNames(MaxNumFilters);

int HandleDaemon(int ClientSock) {
  pthread_mutex_lock(&Mutex);
  assert(find(DaemonSocks.begin(), DaemonSocks.end(), ClientSock) ==
         DaemonSocks.end());
  DaemonSocks.push_back(ClientSock);
  pthread_mutex_unlock(&Mutex);

  char Response[MaxBufferSize];
  while (ReceiveMessage(ClientSock, Response) == 0) {
    pthread_mutex_lock(&Mutex);
    if (CtrlClientSock == -1) {
      errs() << "Loom controller client is not started yet\n";
    } else {
      // Forward the response to the controller client. The controller client
      // may receive multiple responses from a multiprocess program. We could
      // have better handle this situation by sending OK only when all processes
      // have successfully installed the filter, and deleting the filter if any
      // process fails.
      SendMessage(CtrlClientSock, Response);
    }
    pthread_mutex_unlock(&Mutex);
  }

  // Remove <ClientSock> from <DaemonSocks>.
  pthread_mutex_lock(&Mutex);
  remove(DaemonSocks.begin(), DaemonSocks.end(), ClientSock);
  pthread_mutex_unlock(&Mutex);

  return 0;
}

void HandleAddFilter(const string &FilterFileName) {
  if (find(FilterFileNames.begin(), FilterFileNames.end(), FilterFileName) !=
      FilterFileNames.end()) {
    SendMessage(CtrlClientSock, "this filter is already installed");
    return;
  }

  // find the first unused ID
  vector<string>::iterator Pos = find(FilterFileNames.begin(),
                                      FilterFileNames.end(),
                                      "");
  if (Pos == FilterFileNames.end()) {
    SendMessage(CtrlClientSock, "too many filters");
    return;
  }
  *Pos = FilterFileName;

  ostringstream OS;
  OS << "add " << (Pos - FilterFileNames.begin()) << " " << FilterFileName;
  pthread_mutex_lock(&Mutex);
  for (size_t i = 0; i < DaemonSocks.size(); ++i) {
    SendMessage(DaemonSocks[i], OS.str().c_str());
  }
  pthread_mutex_unlock(&Mutex);
}

void HandleDeleteFilter(unsigned FilterID) {
  if (FilterFileNames[FilterID] == "") {
    SendMessage(CtrlClientSock, "this ID does not exist");
    return;
  }
  FilterFileNames[FilterID] = "";

  ostringstream OS;
  OS << "del " << FilterID;
  pthread_mutex_lock(&Mutex);
  for (size_t i = 0; i < DaemonSocks.size(); ++i) {
    SendMessage(DaemonSocks[i], OS.str().c_str());
  }
  pthread_mutex_unlock(&Mutex);
}

void HandleListFilters() {
  ostringstream OS;
  bool First = true;
  for (size_t i = 0; i < FilterFileNames.size(); ++i) {
    if (FilterFileNames[i] != "") {
      if (First)
        First = false;
      else
        OS << "\n";
      OS << i << "\t" << FilterFileNames[i];
    }
  }
  SendMessage(CtrlClientSock, OS.str().c_str());
}

int HandleControllerClient(int ClientSock) {
  pthread_mutex_lock(&Mutex);
  if (CtrlClientSock != -1) {
    errs() << "another Loom controller client is running\n";
    pthread_mutex_unlock(&Mutex);
    return -1;
  }
  CtrlClientSock = ClientSock;
  pthread_mutex_unlock(&Mutex);

  char Cmd[MaxBufferSize];
  // benign race when reading <CtrlClientSock>
  while (ReceiveMessage(CtrlClientSock, Cmd) == 0) {
    istringstream IS(Cmd);
    string Op;
    if (!(IS >> Op)) {
      SendMessage(CtrlClientSock, "wrong format");
      continue;
    }
    if (Op == "add") {
      string FilterFileName;
      if (!(IS >> FilterFileName)) {
        SendMessage(CtrlClientSock, "wrong format");
        continue;
      }
      HandleAddFilter(FilterFileName);
    } else if (Op == "del") {
      unsigned FilterID;
      if (!(IS >> FilterID)) {
        SendMessage(CtrlClientSock, "wrong format");
        continue;
      }
      HandleDeleteFilter(FilterID);
    } else if (Op == "ls") {
      HandleListFilters();
    } else {
      SendMessage(CtrlClientSock, "unknown command");
    }
  }

  // Reset <CtrlClientSock> to allow another controller.
  pthread_mutex_lock(&Mutex);
  CtrlClientSock = -1;
  pthread_mutex_unlock(&Mutex);

  return 0;
}

void *HandleClient(void *Arg) {
  int ClientSock = (intptr_t)Arg;

  // Identify the client.
  char Buffer[MaxBufferSize];
  if (ReceiveMessage(ClientSock, Buffer) == -1) {
    close(ClientSock);
    return (void *)-1;
  }

  int Ret = 0;
  if (strcmp(Buffer, "iam loom_daemon") == 0) {
    outs() << "which is a Loom daemon\n";
    Ret = HandleDaemon(ClientSock);
  } else if (strcmp(Buffer, "iam loom_ctl") == 0) {
    outs() << "which is a Loom controller client\n";
    Ret = HandleControllerClient(ClientSock);
  } else {
    outs() << "which is an unknown client. ";
    outs() << "close the connection immediately\n";
  }

  close(ClientSock);
  return (void *)Ret;
}

int RunControllerServer() {
  int AcceptSock = socket(AF_INET, SOCK_STREAM, 0);
  if (AcceptSock == -1) {
    perror("socket");
    return -1;
  }
  int OptionVal = 1;
  if (setsockopt(AcceptSock, SOL_SOCKET,
                 SO_REUSEADDR, &OptionVal, sizeof(int)) == -1) {
    perror("setsockopt");
    return -1;
  }

  struct sockaddr_in ServerAddr;
  bzero(&ServerAddr, sizeof(ServerAddr));
  ServerAddr.sin_family = AF_INET;
  ServerAddr.sin_port = htons(CONTROLLER_PORT);
  ServerAddr.sin_addr.s_addr = INADDR_ANY;
  if (bind(AcceptSock,
           (struct sockaddr *)&ServerAddr, sizeof(ServerAddr)) == -1) {
    perror("bind");
    return -1;
  }

  if (listen(AcceptSock, 5) == -1) {
    perror("listen");
    return -1;
  }

  outs() << "Loom controller started. ";
  outs() << "Press Ctrl+C to exit...\n";
  struct sockaddr_in ClientAddr;
  socklen_t ClientAddrLen = sizeof(ClientAddr);
  while (true) {
    int ClientSock = accept(AcceptSock,
                            (struct sockaddr *)&ClientAddr, &ClientAddrLen);
    if (ClientSock == -1) {
      perror("accept");
      return -1;
    }

    outs() << "connected by " << inet_ntoa(ClientAddr.sin_addr) << ":" <<
        ntohs(ClientAddr.sin_port) << ", ";
    pthread_t child;
    if (pthread_create(&child, NULL, HandleClient, (void *)ClientSock) != 0) {
      perror("pthread_create");
      return -1;
    }
  }

  return 0;
}

int CommandAddFilter(int CtrlServerSock, const string &FilterFileName) {
  ostringstream OS;
  OS << "add " << FilterFileName;
  return SendMessage(CtrlServerSock, OS.str().c_str());
}

int CommandDeleteFilter(int CtrlServerSock, unsigned FilterID) {
  ostringstream OS;
  OS << "del " << FilterID;
  return SendMessage(CtrlServerSock, OS.str().c_str());
}

int CommandListFilters(int CtrlServerSock) {
  return SendMessage(CtrlServerSock, "ls");
}

int RunControllerClient() {
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
    if (Args.size() != 0) {
      errs() << "wrong format\n";
      Ret = -1;
    } else {
      Ret = CommandListFilters(CtrlServerSock);
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

int main(int argc, char *argv[]) {
  cl::ParseCommandLineOptions(argc, argv, "Loom controller");
  if (ControllerAction == "server") {
    if (RunControllerServer() == -1)
      return 1;
  } else if (ControllerAction == "add" ||
             ControllerAction == "delete" ||
             ControllerAction == "ls") {
    if (RunControllerClient() == -1)
      return 1;
  } else {
    errs() << "Not implemented yet\n";
  }
  return 0;
}

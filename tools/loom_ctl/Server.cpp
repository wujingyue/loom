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

// protect <CtrlClientSock> and <Daemons>
static pthread_mutex_t Mutex = PTHREAD_MUTEX_INITIALIZER;
static int CtrlClientSock = -1;
static map<pid_t, int> Daemons;

// needn't be protected, because it is only used by HandleControllerClient.
static vector<string> FilterFileNames(MaxNumFilters);

static int HandleDaemon(int ClientSock, pid_t PID) {
  pthread_mutex_lock(&Mutex);
  Daemons[PID] = ClientSock;
  pthread_mutex_unlock(&Mutex);

  char Response[MaxBufferSize];
  while (ReceiveMessage(ClientSock, Response) == 0) {
    pthread_mutex_lock(&Mutex);
    if (CtrlClientSock == -1) {
      errs() << "Loom controller client is not started yet\n";
      pthread_mutex_unlock(&Mutex);
      continue;
    }
    int Sock = CtrlClientSock;
    pthread_mutex_unlock(&Mutex);

    // Forward the response to the controller client.
    // TODO: The controller client may receive multiple responses from a
    // multiprocess program. We could have better handle this situation by
    // sending OK only when all processes have successfully installed the
    // filter, and deleting the filter if any process fails.
    SendMessage(Sock, Response);
  }

  // Remove <ClientSock> from <Daemons>.
  outs() << "Loom daemon for process " << PID << " exits.\n";
  pthread_mutex_lock(&Mutex);
  for (map<pid_t, int>::iterator I = Daemons.begin(); I != Daemons.end(); ++I) {
    if (I->second == ClientSock) {
      Daemons.erase(I);
      break;
    }
  }
  pthread_mutex_unlock(&Mutex);

  return 0;
}

static unsigned getFilterID(const string &FilterFileName) {
  // Reuse the filter ID if this filter is already installed.
  vector<string>::iterator Pos = find(FilterFileNames.begin(),
                                      FilterFileNames.end(),
                                      FilterFileName);
  if (Pos != FilterFileNames.end())
    return Pos - FilterFileNames.begin();

  // Find the first unused ID.
  Pos = find(FilterFileNames.begin(), FilterFileNames.end(), "");
  if (Pos == FilterFileNames.end()) {
    SendMessage(CtrlClientSock, "too many filters");
    return -1;
  }
  *Pos = FilterFileName;
  return Pos - FilterFileNames.begin();
}

static void HandleAddFilter(pid_t PID, const string &FilterFileName) {
  unsigned FilterID = getFilterID(FilterFileName);
  if (FilterID == (unsigned)-1)
    return;

  pthread_mutex_lock(&Mutex);
  if (!Daemons.count(PID)) {
    pthread_mutex_unlock(&Mutex);
    SendMessage(CtrlClientSock, "no such process");
    return;
  }
  int DaemonSock = Daemons[PID];
  pthread_mutex_unlock(&Mutex);

  ostringstream OS;
  OS << "add " << FilterID << " " << FilterFileName;
  if (SendMessage(DaemonSock, OS.str().c_str()) == -1)
    SendMessage(CtrlClientSock, "failed to communicate with this process");
  // otherwise, expect the daemon to send the response back
}

static void HandleDeleteFilter(pid_t PID, unsigned FilterID) {
  if (FilterID >= MaxNumFilters) {
    SendMessage(CtrlClientSock, "invalid ID");
    return;
  }
  if (FilterFileNames[FilterID] == "") {
    SendMessage(CtrlClientSock, "no such filter ID");
    return;
  }
  // TODO: a filter may still be used after the controller deletes it from one
  // process but not all. For simplicity, we never reclaim a filter ID. This
  // could be improved by keeping track of how many processes are using a
  // filter.

  pthread_mutex_lock(&Mutex);
  if (!Daemons.count(PID)) {
    pthread_mutex_unlock(&Mutex);
    SendMessage(CtrlClientSock, "no such process");
    return;
  }
  int DaemonSock = Daemons[PID];
  pthread_mutex_unlock(&Mutex);

  ostringstream OS;
  OS << "del " << FilterID;
  if (SendMessage(DaemonSock, OS.str().c_str()) == -1)
    SendMessage(CtrlClientSock, "failed to communicate with this process");
  // otherwise, expect the daemon to send the response back
}

static void HandleListFilters(pid_t PID = -1) {
  if (PID == -1) {
    ostringstream OS;
    OS << "ID\tfile";
    for (size_t i = 0; i < FilterFileNames.size(); ++i) {
      if (FilterFileNames[i] != "") {
        OS << "\n" << i << "\t" << FilterFileNames[i];
      }
    }
    SendMessage(CtrlClientSock, OS.str().c_str());
    return;
  }

  pthread_mutex_lock(&Mutex);
  if (!Daemons.count(PID)) {
    SendMessage(CtrlClientSock, "no such process");
    pthread_mutex_unlock(&Mutex);
    return;
  }
  SendMessage(Daemons[PID], "ls");
  pthread_mutex_unlock(&Mutex);
}

static void HandleListDaemons() {
  ostringstream OS;
  OS << "PID\tsocket";
  for (map<pid_t, int>::iterator I = Daemons.begin(); I != Daemons.end(); ++I) {
    OS << "\n" << I->first << "\t" << I->second;
  }
  SendMessage(CtrlClientSock, OS.str().c_str());
}

static int HandleControllerClient(int ClientSock) {
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
      pid_t PID;
      string FilterFileName;
      if (!(IS >> PID >> FilterFileName)) {
        SendMessage(CtrlClientSock, "wrong format");
        continue;
      }
      HandleAddFilter(PID, FilterFileName);
    } else if (Op == "del") {
      pid_t PID;
      unsigned FilterID;
      if (!(IS >> PID >> FilterID)) {
        SendMessage(CtrlClientSock, "wrong format");
        continue;
      }
      HandleDeleteFilter(PID, FilterID);
    } else if (Op == "ls") {
      unsigned PID;
      if (!(IS >> PID))
        HandleListFilters();
      else
        HandleListFilters(PID);
    } else if (Op == "ps") {
      HandleListDaemons();
    } else {
      SendMessage(CtrlClientSock, "unknown command");
    }
  }

  outs() << "Loom controller client exits.\n";
  // Reset <CtrlClientSock> to allow another controller.
  pthread_mutex_lock(&Mutex);
  CtrlClientSock = -1;
  pthread_mutex_unlock(&Mutex);

  return 0;
}

static void *HandleClient(void *Arg) {
  int ClientSock = (intptr_t)Arg;

  // Identify the client.
  char Buffer[MaxBufferSize];
  if (ReceiveMessage(ClientSock, Buffer) == -1) {
    close(ClientSock);
    return (void *)-1;
  }

  int Ret = 0;
  if (strncmp(Buffer, "iam loom_daemon", strlen("iam loom_daemon")) == 0) {
    outs() << "conntected by a Loom daemon\n";
    pid_t PID;
    if (sscanf(Buffer, "iam loom_daemon %d", &PID) != 1)
      Ret = -1;
    else
      Ret = HandleDaemon(ClientSock, PID);
  } else if (strcmp(Buffer, "iam loom_ctl") == 0) {
    outs() << "connected by a Loom controller client\n";
    Ret = HandleControllerClient(ClientSock);
  } else {
    outs() << "connected by an unknown client. ";
    outs() << "close the connection immediately\n";
  }

  close(ClientSock);
  return (void *)Ret;
}

int loom::RunControllerServer() {
  // Ignore SIGPIPE.
  signal(SIGPIPE, SIG_IGN);

  int AcceptSock = socket(AF_INET, SOCK_STREAM, 0);
  if (AcceptSock == -1) {
    perror("socket");
    return -1;
  }
  int TrueVal = 1;
  if (setsockopt(AcceptSock, SOL_SOCKET,
                 SO_REUSEADDR, &TrueVal, sizeof(int)) == -1) {
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

    pthread_t child;
    if (pthread_create(&child, NULL, HandleClient, (void *)ClientSock) != 0) {
      perror("pthread_create");
      return -1;
    }
  }

  return 0;
}

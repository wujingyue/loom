#include <pthread.h>
#include <stdio.h>
#include <arpa/inet.h>

#include <vector>

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include "loom/config.h"
#include "loom/Utils.h"

using namespace std;
using namespace llvm;

// TODO: find a way to display more description on "controller action"
static cl::opt<string> ControllerAction(cl::Positional,
                                        cl::desc("<controller action>"),
                                        cl::Required);

// protect <CtrlClientSock> and <DaemonSocks>
static pthread_mutex_t Mutex = PTHREAD_MUTEX_INITIALIZER;
static int CtrlClientSock = -1;
static vector<int> DaemonSocks;

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

int HandleLoomController(int ClientSock) {
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
    // Get a local copy of <DaemonSocks> to avoid race conditions.
    pthread_mutex_lock(&Mutex);
    vector<int> CopyOfDaemonSocks(DaemonSocks);
    pthread_mutex_unlock(&Mutex);
    // Forward the command to all daemons.
    for (size_t i = 0; i < CopyOfDaemonSocks.size(); ++i)
      SendMessage(CopyOfDaemonSocks[i], Cmd);
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
    Ret = HandleDaemon(ClientSock);
  } else if (strcmp(Buffer, "iam loom_ctl") == 0) {
    Ret = HandleLoomController(ClientSock);
  }
  close(ClientSock);
  return (void *)Ret;
}

int RunControllerServer() {
  int AcceptSock = socket(AF_INET, SOCK_STREAM, 0);
  if (AcceptSock == -1) {
    perror("socket");
    return 1;
  }
  int OptionVal = 1;
  if (setsockopt(AcceptSock, SOL_SOCKET,
                 SO_REUSEADDR, &OptionVal, sizeof(int)) == -1) {
    perror("setsockopt");
    return 1;
  }

  struct sockaddr_in ServerAddr;
  bzero(&ServerAddr, sizeof(ServerAddr));
  ServerAddr.sin_family = AF_INET;
  ServerAddr.sin_port = htons(CONTROLLER_PORT);
  ServerAddr.sin_addr.s_addr = INADDR_ANY;
  if (bind(AcceptSock,
           (struct sockaddr *)&ServerAddr, sizeof(ServerAddr)) == -1) {
    perror("bind");
    return 1;
  }

  if (listen(AcceptSock, 5) == -1) {
    perror("listen");
    return 1;
  }

  errs() << "Loom controller started. ";
  errs() << "Press Ctrl+C to exit...\n";
  struct sockaddr_in ClientAddr;
  socklen_t ClientAddrLen = sizeof(ClientAddr);
  while (true) {
    int ClientSock = accept(AcceptSock,
                            (struct sockaddr *)&ClientAddr, &ClientAddrLen);
    if (ClientSock == -1) {
      perror("accept");
      return 1;
    }

    pthread_t child;
    if (pthread_create(&child, NULL, HandleClient, (void *)ClientSock) != 0) {
      perror("pthread_create");
      return 1;
    }
  }

  return 0;
}

int main(int argc, char *argv[]) {
  cl::ParseCommandLineOptions(argc, argv, "Loom controller");
  if (ControllerAction == "server") {
    return RunControllerServer();
  }
  errs() << "Not implemented yet\n";
  return 0;
}

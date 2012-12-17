#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <signal.h>
#include <stdint.h>
#include <strings.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/prctl.h>

#include "loom/config.h"
#include "UpdateEngine.h"

static const int MaxBufferSize = 1024;

static int CtrlSock = -1;

static int BlockAllSignals() {
  sigset_t SigSet;
  if (sigfillset(&SigSet) == -1) {
    perror("sigfillset");
    return -1;
  }

  if (pthread_sigmask(SIG_BLOCK, &SigSet, NULL) != 0) {
    perror("pthread_sigmask");
    return -1;
  }

  return 0;
}

static void SetThreadName() {
  if (prctl(PR_SET_NAME, "loom-daemon", 0, 0, 0) == -1) {
    perror("prctl");
  }
}

static int CreateSocketToController() {
  int Sock = socket(AF_INET, SOCK_STREAM, 0);
  if (Sock == -1) {
    perror("socket");
    return -1;
  }
  struct sockaddr_in ServerAddr;
  bzero(&ServerAddr, sizeof ServerAddr);
  ServerAddr.sin_family = AF_INET;
  ServerAddr.sin_addr.s_addr = inet_addr(CONTROLLER_IP);
  ServerAddr.sin_port = htons(CONTROLLER_PORT);
  if (connect(Sock, (struct sockaddr *)&ServerAddr, sizeof ServerAddr) == -1) {
    perror("Failed to connect to the controller");
    return -1;
  }
  return Sock;
}

static int SendExactly(int Sock, const void *Buffer, size_t L) {
  size_t Sent = 0;
  while (Sent < L) {
    ssize_t R = send(Sock, (char *)Buffer + Sent, L - Sent, 0);
    if (R == -1) {
      perror("send");
      return -1;
    }
    Sent += R;
  }
  return 0;
}

static int ReceiveExactly(int Sock, void *Buffer, size_t L) {
  size_t Received = 0;
  while (Received < L) {
    ssize_t R = recv(Sock, (char *)Buffer + Received, L - Received, 0);
    if (R == -1) {
      perror("send");
      return -1;
    }
    Received += R;
  }
  return 0;
}

static int SendToController(const char *M) {
  uint32_t L = htonl(strlen(M));
  if (SendExactly(CtrlSock, &L, sizeof(uint32_t)) == -1)
    return -1;
  if (SendExactly(CtrlSock, M, strlen(M)) == -1)
    perror("send");
  return 0;
}

static int ReceiveFromController(char *M) {
  uint32_t L;
  if (ReceiveExactly(CtrlSock, &L, sizeof(int)) == -1)
    return -1;
  L = ntohl(L);
  if (L >= MaxBufferSize) {
    fprintf(stderr, "Message too long: length = %u\n", L);
    return -1;
  }
  if (ReceiveExactly(CtrlSock, M, L) == -1)
    return -1;
  M[L] = '\0';
  return 0;
}

int ProcessMessage(char *Buffer, char *Response) {
  char *Cmd = strtok(Buffer, " ");
  if (Cmd == NULL) {
    sprintf(Response, "[Error] no command specified");
    return -1;
  }

  if (strcmp(Cmd, "add") == 0) {
    char *Token = strtok(NULL, " ");
    if (Token == NULL) {
      sprintf(Response, "[Error] format error: add <fix ID> <file name>");
      return -1;
    }
    int FixID = atoi(Token);
    char *FileName = strtok(NULL, " ");
    if (FileName == NULL) {
      sprintf(Response, "[Error] format error: add <fix ID> <file name>");
      return -1;
    }
    if (AddFix(FixID, FileName) == -1) {
      sprintf(Response, "[Error] failed to add the fix");
      return -1;
    }
    sprintf(Response, "OK");
  } else if (strcmp(Cmd, "del") == 0) {
    char *Token = strtok(NULL, " ");
    if (Token == NULL) {
      sprintf(Response, "[Error] format error: del <fix ID>");
      return -1;
    }
    int FixID = atoi(Token);
    if (DeleteFix(FixID) == -1) {
      sprintf(Response, "[Error] failed to delete the fix");
      return -1;
    }
    sprintf(Response, "OK");
  } else {
    sprintf(Response, "[Error] unknown command");
    return -1;
  }
  return 0;
}

void *RunDaemon(void *) {
  fprintf(stderr, "daemon is running...\n");

  // Block all signals. Applications such as MySQL and Apache have their own way
  // of handling signals, which we do not want to interfere. For instance, MySQL
  // has a special signal handling thread, which calls sigwait to wait for
  // signals. If the Loom daemon stole the signal, the sigwait would never
  // return, and the server would not be killed.
  if (BlockAllSignals() == -1)
    return (void *)-1;

  // Set the thread name, so that we can "ps c" to view it.
  SetThreadName();

  CtrlSock = CreateSocketToController();
  if (CtrlSock == -1)
    return (void *)-1;

  fprintf(stderr, "Loom daemon is connected to Loom controller\n");
  while (1) {
    char Buffer[MaxBufferSize];
    if (ReceiveFromController(Buffer) == -1)
      return (void *)-1;
    char Response[MaxBufferSize] = {'\0'};
    ProcessMessage(Buffer, Response);
    assert(strlen(Response) > 0 && "empty response");
    if (SendToController(Response) == -1)
      return (void *)-1;
  }

  // unreachable
  assert(false);

  return NULL;
}

int StartDaemon() {
  pthread_t DaemonTID;
  if (pthread_create(&DaemonTID, NULL, RunDaemon, NULL) == -1) {
    perror("pthread_create");
    return -1;
  }

  fprintf(stderr, "Daemon TID = %lu\n", DaemonTID);
  return 0;
}

int StopDaemon() {
  fprintf(stderr, "StopDaemon\n");
  // The daemon thread will be automatically killed by the parent process. No
  // need to explicitly kill it.
  // TODO: issue a warning if the daemon already exits.
  return 0;
}

void EvacuateAndUpdate(unsigned *UnsafeBackEdges, unsigned NumUnsafeBackEdges,
                       unsigned *UnsafeCallSites, unsigned NumUnsafeCallSites) {
  // Turn on wait flags for all safe back edges.
  int Unsafe[MaxNumBackEdges];
  memset(Unsafe, 0, sizeof(Unsafe));
  for (unsigned i = 0; i < NumUnsafeBackEdges; ++i)
    Unsafe[UnsafeBackEdges[i]] = true;
  for (unsigned i = 0; i < MaxNumBackEdges; ++i) {
    if (!Unsafe[i])
      LoomWait[i] = true;
  }

  // Make sure nobody is running inside an unsafe call site.
  while (true) {
    pthread_rwlock_wrlock(&LoomUpdateLock);
    bool InBlockingCallSite = false;
    for (unsigned i = 0; i < NumUnsafeCallSites; ++i) {
      if (LoomCounter[i] > 0) {
        InBlockingCallSite = true;
        break;
      }
    }
    if (!InBlockingCallSite) {
      break;
    }
    pthread_rwlock_unlock(&LoomUpdateLock);
  }

  // Update.

  // Restore wait flags and counters.
  memset((void *)LoomWait, 0, sizeof(LoomWait));

  // Resume application threads.
  pthread_rwlock_unlock(&LoomUpdateLock);
}

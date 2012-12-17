#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdint.h>
#include <strings.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/prctl.h>

#include "loom/config.h"
#include "UpdateEngine.h"

#define MaxBufferSize (1024)

struct Filter {
  enum Type {
    Unknown = 0,
    CriticalRegion,
  };
  enum Type FilterType;
  struct Operation *Ops;
  unsigned NumOps;
};

static int CtrlSock = -1;
static struct Filter Filters[MaxNumFilters];

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
    perror("failed to connect to the controller");
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
    if (R == 0) {
      fprintf(stderr, "remote socket closed\n");
      return -1;
    }
    if (R == -1) {
      perror("recv");
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
    fprintf(stderr, "message too long: length = %u\n", L);
    return -1;
  }
  if (ReceiveExactly(CtrlSock, M, L) == -1)
    return -1;
  M[L] = '\0';
  return 0;
}

void InitFilters() {
  for (unsigned i = 0; i < MaxNumFilters; ++i) {
    Filters[i].FilterType = Unknown;
  }
}

static int ReadFilter(unsigned FilterID,
                      const char *FileName,
                      struct Filter *F) {
  FILE *FilterFile = fopen(FileName, "r");
  if (!FilterFile) {
    fprintf(stderr, "cannot open filter file %s\n", FileName);
    return -1;
  }
  int NumericFilterType;
  if (fscanf(FilterFile, "%d %u\n", &NumericFilterType, &F->NumOps) != 2) {
    fprintf(stderr, "format error in filter file %s\n", FileName);
    fclose(FilterFile);
    return -1;
  }
  F->FilterType = NumericFilterType;
  F->Ops = calloc(F->NumOps, sizeof(struct Operation));
  for (unsigned i = 0; i < F->NumOps; ++i) {
    int EntryOrExit;
    unsigned SlotID;
    if (fscanf(FilterFile, "%d %u\n", &EntryOrExit, &SlotID) != 2) {
      fprintf(stderr, "format error in filter file %s\n", FileName);
      fclose(FilterFile);
      return -1;
    }
    switch (F->FilterType) {
      case CriticalRegion:
        {
          struct Operation *Op = &F->Ops[i];
          Op->CallBack = (EntryOrExit == 0 ?
                          EnterCriticalRegion :
                          ExitCriticalRegion);
          Op->Arg = (void *)FilterID;
          Op->SlotID = SlotID;
        }
        break;
      default:
        fprintf(stderr, "unknown filter type\n");
        return -1;
    }
  }
  fclose(FilterFile);
  return 0;
}

static void Evacuate(const unsigned *UnsafeBackEdges,
                     unsigned NumUnsafeBackEdges,
                     const unsigned *UnsafeCallSites,
                     unsigned NumUnsafeCallSites) {
  // Turn on wait flags for all safe back edges.
  int Unsafe[MaxNumBackEdges];
  memset(Unsafe, 0, sizeof(Unsafe));
  for (unsigned i = 0; i < NumUnsafeBackEdges; ++i)
    Unsafe[UnsafeBackEdges[i]] = 1;
  for (unsigned i = 0; i < MaxNumBackEdges; ++i) {
    if (!Unsafe[i])
      LoomWait[i] = 1;
  }

  // Make sure nobody is running inside an unsafe call site.
  while (1) {
    pthread_rwlock_wrlock(&LoomUpdateLock);
    int InBlockingCallSite = 0;
    for (unsigned i = 0; i < NumUnsafeCallSites; ++i) {
      if (LoomCounter[UnsafeCallSites[i]] > 0) {
        InBlockingCallSite = 1;
        break;
      }
    }
    if (!InBlockingCallSite) {
      break;
    }
    pthread_rwlock_unlock(&LoomUpdateLock);
  }
}

static void Resume() {
  // Restore wait flags and counters.
  memset((void *)LoomWait, 0, sizeof(LoomWait));
  // Resume application threads.
  pthread_rwlock_unlock(&LoomUpdateLock);
}

static int AddFilter(int FilterID, const char *FileName) {
  if (Filters[FilterID].FilterType != Unknown) {
    fprintf(stderr, "filter %d already exists\n", FilterID);
    return -1;
  }

  struct Filter F;
  if (ReadFilter(FilterID, FileName, &F) == -1)
    return -1;

  unsigned UnsafeBackEdges[MaxNumBackEdges];
  unsigned UnsafeCallSites[MaxNumBlockingCS];
  // TODO: compute unsafe back edges and call sites from the filter

  Evacuate(UnsafeBackEdges, 0, UnsafeCallSites, 0);

  switch (F.FilterType) {
    case CriticalRegion:
      pthread_mutex_init(&Mutexes[FilterID], NULL);
      for (unsigned i = 0; i < F.NumOps; ++i) {
        PrependOperation(&F.Ops[i], &LoomOperations[F.Ops[i].SlotID]);
      }
      break;
    default:
      assert(0 && "should be already handled in ReadFilter");
  }

  Filters[FilterID] = F;

  Resume();

  return 0;
}

static int DeleteFilter(int FilterID) {
  struct Filter *F = &Filters[FilterID];
  if (F->FilterType == Unknown) {
    fprintf(stderr, "filter %d does not exist\n", FilterID);
    return -1;
  }

  unsigned UnsafeBackEdges[MaxNumBackEdges];
  unsigned UnsafeCallSites[MaxNumBlockingCS];
  // TODO: compute unsafe back edges and call sites from the filter

  Evacuate(UnsafeBackEdges, 0, UnsafeCallSites, 0);

  switch (F->FilterType) {
    case CriticalRegion:
      pthread_mutex_destroy(&Mutexes[FilterID]);
      for (unsigned i = 0; i < F->NumOps; ++i)
        UnlinkOperation(&F->Ops[i], &LoomOperations[F->Ops[i].SlotID]);
      break;
    default:
      fprintf(stderr, "unknown filter type\n");
      return -1;
  }

  free(F->Ops);
  F->FilterType = Unknown;

  Resume();

  return 0;
}

static int ProcessMessage(char *Buffer, char *Response) {
  char *Cmd = strtok(Buffer, " ");
  if (Cmd == NULL) {
    sprintf(Response, "no command specified");
    return -1;
  }

  if (strcmp(Cmd, "add") == 0) {
    char *Token = strtok(NULL, " ");
    if (Token == NULL) {
      sprintf(Response, "format error: add <filter ID> <file name>");
      return -1;
    }
    int FilterID = atoi(Token);
    char *FileName = strtok(NULL, " ");
    if (FileName == NULL) {
      sprintf(Response, "format error: add <filter ID> <file name>");
      return -1;
    }
    if (AddFilter(FilterID, FileName) == -1) {
      sprintf(Response, "failed to add the filter");
      return -1;
    }
    sprintf(Response, "OK");
  } else if (strcmp(Cmd, "del") == 0) {
    char *Token = strtok(NULL, " ");
    if (Token == NULL) {
      sprintf(Response, "format error: del <filter ID>");
      return -1;
    }
    int FilterID = atoi(Token);
    if (DeleteFilter(FilterID) == -1) {
      sprintf(Response, "failed to delete the filter");
      return -1;
    }
    sprintf(Response, "OK");
  } else {
    sprintf(Response, "unknown command");
    return -1;
  }
  return 0;
}

static void *RunDaemon(void *Arg) {
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
  assert(0 && "unreachable");

  return NULL;
}

int StartDaemon() {
  pthread_t DaemonTID;
  if (pthread_create(&DaemonTID, NULL, RunDaemon, NULL) == -1) {
    perror("pthread_create");
    return -1;
  }

  fprintf(stderr, "daemon TID = %lu\n", DaemonTID);
  return 0;
}

int StopDaemon() {
  fprintf(stderr, "StopDaemon\n");
  // The daemon thread will be automatically killed by the parent process. No
  // need to explicitly kill it.
  // TODO: issue a warning if the daemon already exits.
  return 0;
}

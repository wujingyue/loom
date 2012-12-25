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
#include "loom/Utils.h"
#include "UpdateEngine.h"

struct Filter {
  enum Type {
    Unknown = 0,
    CriticalRegion
  } FilterType;

  unsigned NumOps;
  struct Operation *Ops;

  unsigned NumFuncsToPatch;
  unsigned *FuncsToPatch;

  unsigned NumUnsafeBackEdges;
  unsigned *UnsafeBackEdges;

  unsigned NumUnsafeCallSites;
  unsigned *UnsafeCallSites;
};

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
  struct sockaddr_in ServerAddr;
  int Sock;
  Sock = socket(AF_INET, SOCK_STREAM, 0);
  if (Sock == -1) {
    perror("socket");
    return -1;
  }
  bzero(&ServerAddr, sizeof ServerAddr);
  ServerAddr.sin_family = AF_INET;
  ServerAddr.sin_addr.s_addr = inet_addr(CONTROLLER_IP);
  ServerAddr.sin_port = htons(CONTROLLER_PORT);
  if (connect(Sock, (struct sockaddr *)&ServerAddr, sizeof ServerAddr) == -1) {
    perror("failed to connect to the controller server");
    return -1;
  }
  return Sock;
}

void InitFilters() {
  unsigned i;
  for (i = 0; i < MaxNumFilters; ++i) {
    Filters[i].FilterType = Unknown;
  }
}

static int ReadFilter(unsigned FilterID,
                      const char *FileName,
                      struct Filter *F) {
  FILE *FilterFile = NULL;
  int NumericFilterType;
  unsigned i;

  F->FilterType = Unknown;
  F->Ops = NULL;
  F->FuncsToPatch = NULL;
  F->UnsafeBackEdges = NULL;
  F->UnsafeCallSites = NULL;

  FilterFile = fopen(FileName, "r");
  if (!FilterFile) {
    fprintf(stderr, "cannot open filter file %s\n", FileName);
    return -1;
  }

  if (fscanf(FilterFile, "%d", &NumericFilterType) != 1)
    goto format_error;
  F->FilterType = NumericFilterType;

  if (fscanf(FilterFile, "%u", &F->NumOps) != 1)
    goto format_error;
  F->Ops = calloc(F->NumOps, sizeof(struct Operation));
  // TODO: check the return value of calloc

  for (i = 0; i < F->NumOps; ++i) {
    int EntryOrExit;
    unsigned SlotID;
    if (fscanf(FilterFile, "%d %u\n", &EntryOrExit, &SlotID) != 2)
      goto format_error;
    switch (F->FilterType) {
      case CriticalRegion:
        {
          struct Operation *Op = &F->Ops[i];
          Op->CallBack = (EntryOrExit == 0 ?
                          EnterCriticalRegion :
                          ExitCriticalRegion);
          Op->Arg = (void *)(unsigned long)FilterID;
          // TODO: check the range of SlotID
          Op->SlotID = SlotID;
        }
        break;
      default:
        goto format_error;
    }
  }

  if (fscanf(FilterFile, "%u", &F->NumFuncsToPatch) != 1)
    goto format_error;
  F->FuncsToPatch = calloc(F->NumFuncsToPatch, sizeof(unsigned));
  // TODO: check the return value of calloc

  for (i = 0; i < F->NumFuncsToPatch; ++i) {
    if (fscanf(FilterFile, "%u", &F->FuncsToPatch[i]) != 1)
      goto format_error;
  }

  if (fscanf(FilterFile, "%u", &F->NumUnsafeBackEdges) != 1)
    goto format_error;
  F->UnsafeBackEdges = calloc(F->NumUnsafeBackEdges, sizeof(unsigned));
  // TODO: check the return value of calloc
  for (i = 0; i < F->NumUnsafeBackEdges; ++i) {
    if (fscanf(FilterFile, "%u", &F->UnsafeBackEdges[i]) != 1)
      goto format_error;
  }

  if (fscanf(FilterFile, "%u", &F->NumUnsafeCallSites) != 1)
    goto format_error;
  F->UnsafeCallSites = calloc(F->NumUnsafeCallSites, sizeof(unsigned));
  // TODO: check the return value of calloc
  for (i = 0; i < F->NumUnsafeCallSites; ++i) {
    if (fscanf(FilterFile, "%u", &F->UnsafeCallSites[i]) != 1)
      goto format_error;
  }

  fclose(FilterFile);
  return 0;

format_error:
  fprintf(stderr, "wrong format in filter file %s\n", FileName);
  if (F->Ops) free(F->Ops);
  if (F->FuncsToPatch) free(F->FuncsToPatch);
  if (F->UnsafeBackEdges) free(F->UnsafeBackEdges);
  if (F->UnsafeCallSites) free(F->UnsafeCallSites);
  fclose(FilterFile);
  return -1;
}

static void Evacuate(const unsigned *UnsafeBackEdges,
                     unsigned NumUnsafeBackEdges,
                     const unsigned *UnsafeCallSites,
                     unsigned NumUnsafeCallSites) {
  unsigned i;
  /* Turn on wait flags for all safe back edges. */
  int Unsafe[MaxNumBackEdges];
  memset(Unsafe, 0, sizeof(Unsafe));
  for (i = 0; i < NumUnsafeBackEdges; ++i)
    Unsafe[UnsafeBackEdges[i]] = 1;
  for (i = 0; i < MaxNumBackEdges; ++i) {
    if (!Unsafe[i])
      LoomWait[i] = 1;
  }

  /* Make sure nobody is running inside an unsafe call site. */
  while (1) {
    int InBlockingCallSite = 0;
    unsigned i;
    pthread_rwlock_wrlock(&LoomUpdateLock);
    for (i = 0; i < NumUnsafeCallSites; ++i) {
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
  /* Restore wait flags and counters. */
  memset((void *)LoomWait, 0, sizeof(LoomWait));
  /* Resume application threads. */
  pthread_rwlock_unlock(&LoomUpdateLock);
}

static int AddFilter(unsigned FilterID, const char *FileName) {
  struct Filter F;
  unsigned i;
  assert(FilterID < MaxNumFilters);
  if (Filters[FilterID].FilterType != Unknown) {
    fprintf(stderr, "filter %u already exists\n", FilterID);
    return -1;
  }

  if (ReadFilter(FilterID, FileName, &F) == -1)
    return -1;

  Evacuate(F.UnsafeBackEdges, F.NumUnsafeBackEdges,
           F.UnsafeCallSites, F.NumUnsafeCallSites);

  // Switch the functions to be patched to the slow path.
  for (i = 0; i < F.NumFuncsToPatch; ++i) {
    assert(F.FuncsToPatch[i] < MaxNumFuncs);
    LoomSwitches[F.FuncsToPatch[i]] = 1;
  }

  switch (F.FilterType) {
    case CriticalRegion:
      pthread_mutex_init(&Mutexes[FilterID], NULL);
      for (i = 0; i < F.NumOps; ++i) {
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

static void EraseFilter(struct Filter *F) {
  unsigned i;
  assert(F->FilterType != Unknown);
  F->FilterType = Unknown;
  for (i = 0; i < F->NumOps; ++i)
    UnlinkOperation(&F->Ops[i], &LoomOperations[F->Ops[i].SlotID]);
  free(F->Ops);
  free(F->FuncsToPatch);
  free(F->UnsafeBackEdges);
  free(F->UnsafeCallSites);
}

static int DeleteFilter(unsigned FilterID) {
  struct Filter *F = &Filters[FilterID];

  assert(FilterID < MaxNumFilters);
  if (F->FilterType == Unknown) {
    fprintf(stderr, "filter %u does not exist\n", FilterID);
    return -1;
  }

  Evacuate(F->UnsafeBackEdges, F->NumUnsafeBackEdges,
           F->UnsafeCallSites, F->NumUnsafeCallSites);

  // TODO: We could switch functions back to the slow path if we kept track of
  // how many filters are patching each function.

  switch (F->FilterType) {
    case CriticalRegion:
      pthread_mutex_destroy(&Mutexes[FilterID]);
      break;
    default:
      fprintf(stderr, "unknown filter type\n");
      return -1;
  }
  EraseFilter(F);

  Resume();

  return 0;
}

static unsigned ListFilters(unsigned *FilterIDs, unsigned MaxLen) {
  unsigned i;
  unsigned Len = 0;
  for (i = 0; i < MaxNumFilters; ++i) {
    if (Filters[i].FilterType != Unknown) {
      FilterIDs[Len] = i;
      ++Len;
      if (Len == MaxLen)
        break;
    }
  }
  assert(Len < MaxLen);
  return Len;
}

void ClearFilters() {
  unsigned i;
  for (i = 0; i < MaxNumFilters; ++i) {
    struct Filter *F = &Filters[i];
    if (F->FilterType != Unknown)
      EraseFilter(F);
  }
}

static int ProcessMessage(char *Buffer, char *Response) {
  char *Cmd = strtok(Buffer, " ");
  if (Cmd == NULL) {
    sprintf(Response, "no command specified");
    return -1;
  }

  if (strcmp(Cmd, "add") == 0) {
    char *Token = strtok(NULL, " ");
    unsigned FilterID;
    char *FileName;
    if (Token == NULL) {
      sprintf(Response, "wrong format. expect: add <filter ID> <file name>");
      return -1;
    }
    FilterID = atoi(Token);
    FileName = strtok(NULL, " ");
    if (FileName == NULL) {
      sprintf(Response, "wrong format. expect: add <filter ID> <file name>");
      return -1;
    }
    if (AddFilter(FilterID, FileName) == -1) {
      sprintf(Response, "failed to add the filter");
      return -1;
    }
    sprintf(Response, "filter %u is successfully added", FilterID);
  } else if (strcmp(Cmd, "del") == 0) {
    char *Token = strtok(NULL, " ");
    unsigned FilterID;
    if (Token == NULL) {
      sprintf(Response, "wrong format. expect: del <filter ID>");
      return -1;
    }
    FilterID = atoi(Token);
    if (DeleteFilter(FilterID) == -1) {
      sprintf(Response, "failed to delete the filter");
      return -1;
    }
    sprintf(Response, "filter %u is successfully deleted", FilterID);
  } else if (strcmp(Cmd, "ls") == 0) {
    unsigned FilterIDs[MaxNumFilters];
    unsigned NumFilters = ListFilters(FilterIDs, MaxNumFilters);
    unsigned i;
    unsigned Printed = sprintf(Response, "filters:");
    for (i = 0; i < NumFilters; ++i) {
      Printed += sprintf(Response + Printed, " %u", FilterIDs[i]);
    }
  } else {
    sprintf(Response, "unknown command");
    return -1;
  }
  return 0;
}

static void *RunDaemon(void *Arg) {
  int CtrlSock;
  char Buffer[MaxBufferSize];
  fprintf(stderr, "daemon is running...\n");

  /*
   * Block all signals. Applications such as MySQL and Apache have their own way
   * of handling signals, which we do not want to interfere. For instance, MySQL
   * has a special signal handling thread, which calls sigwait to wait for
   * signals. If the Loom daemon stole the signal, the sigwait would never
   * return, and the server would not be killed.
   */
  if (BlockAllSignals() == -1)
    return (void *)-1;

  /* Set the thread name, so that we can "ps c" to view it. */
  SetThreadName();

  CtrlSock = CreateSocketToController();
  if (CtrlSock == -1)
    return (void *)-1;
  fprintf(stderr, "Loom daemon is connected to Loom controller\n");

  /* Tell the controller "I am a daemon". */
  sprintf(Buffer, "iam loom_daemon %d\n", getpid());
  if (SendMessage(CtrlSock, Buffer) == -1)
    return (void *)-1;
  while (1) {
    char Response[MaxBufferSize] = {'\0'};
    if (ReceiveMessage(CtrlSock, Buffer) == -1)
      return (void *)-1;
    ProcessMessage(Buffer, Response);
    assert(strlen(Response) > 0 && "empty response");
    if (SendMessage(CtrlSock, Response) == -1)
      return (void *)-1;
  }

  /* unreachable */
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
  /*
   * The daemon thread will be automatically killed by the parent process. No
   * need to explicitly kill it.
   * TODO: issue a warning if the daemon already exits.
   */
  return 0;
}

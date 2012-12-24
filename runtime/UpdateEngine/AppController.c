#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "UpdateEngine.h"

volatile int LoomWait[MaxNumBackEdges];
atomic_t LoomCounter[MaxNumBlockingCS];
pthread_rwlock_t LoomUpdateLock;
struct Operation *LoomOperations[MaxNumInsts];

void LoomEnterProcess();
void LoomEnterForkedProcess();
void LoomExitProcess();
void LoomEnterThread();
void LoomExitThread();
void LoomCycleCheck(unsigned BackEdgeID);
void LoomBeforeBlocking(unsigned CallSiteID);
void LoomAfterBlocking(unsigned CallSiteID);
void LoomSlot(unsigned SlotID);

void LoomEnterProcess() {
  fprintf(stderr, "***** LoomEnterProcess *****\n");
  pthread_atfork(NULL, NULL, LoomEnterForkedProcess);
  atexit(LoomExitProcess);
  pthread_rwlock_init(&LoomUpdateLock, NULL);
  memset((void *)LoomWait, 0, sizeof(LoomWait));
  memset((void *)LoomCounter, 0, sizeof(LoomCounter));
  memset((void *)LoomOperations, 0, sizeof(LoomOperations));
  InitFilters();
  if (StartDaemon() == -1) {
    fprintf(stderr, "failed to start the loom daemon. abort...\n");
    exit(1);
  }
  LoomEnterThread();
}

void LoomEnterForkedProcess() {
  fprintf(stderr, "***** LoomEnterForkedProcess *****\n");
  // Reinitialize LoomWait because the Loom daemon is not started yet for this
  // process. Inherit other data structures from the parent process.
  memset((void *)LoomWait, 0, sizeof(LoomWait));
  // Start Loom daemon.
  if (StartDaemon() == -1) {
    fprintf(stderr, "failed to start the loom daemon. abort...\n");
    exit(1);
  }
  // We do not call LoomEnterThread here, because we inherited parent's
  // LoomUpdateLock already.
}

void LoomExitProcess() {
  LoomExitThread();
  if (StopDaemon() == -1) {
    fprintf(stderr, "failed to stop the loom daemon\n");
  }
  ClearFilters();
  for (unsigned i = 0; i < MaxNumInsts; ++i)
    assert(LoomOperations[i] == NULL);
  fprintf(stderr, "***** LoomExitProcess *****\n");
}

void LoomEnterThread() {
  pthread_rwlock_rdlock(&LoomUpdateLock);
}

void LoomExitThread() {
  pthread_rwlock_unlock(&LoomUpdateLock);
}

void LoomCycleCheck(unsigned BackEdgeID) {
  if (LoomWait[BackEdgeID]) {
    pthread_rwlock_unlock(&LoomUpdateLock);
    while (LoomWait[BackEdgeID]);
    pthread_rwlock_rdlock(&LoomUpdateLock);
  }
}

void LoomBeforeBlocking(unsigned CallSiteID) {
  atomic_inc(&LoomCounter[CallSiteID]);
  pthread_rwlock_unlock(&LoomUpdateLock);
}

void LoomAfterBlocking(unsigned CallSiteID) {
  pthread_rwlock_rdlock(&LoomUpdateLock);
  atomic_dec(&LoomCounter[CallSiteID]);
}

void LoomSlot(unsigned SlotID) {
  assert(SlotID < MaxNumInsts);
  for (struct Operation *Op = LoomOperations[SlotID]; Op; Op = Op->Next) {
    Op->CallBack(Op->Arg);
  }
}

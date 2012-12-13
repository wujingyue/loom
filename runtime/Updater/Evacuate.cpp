#include <cstdio>
#include <cstring>
#include <iostream>
#include <set>

#include "Updater.h"

using namespace std;

extern "C" void LoomCycleCheck(unsigned BackEdgeID) {
  if (LoomWait[BackEdgeID]) {
    pthread_rwlock_unlock(&LoomUpdateLock);
    while (LoomWait[BackEdgeID]);
    pthread_rwlock_rdlock(&LoomUpdateLock);
  }
}

extern "C" void LoomBeforeBlocking(unsigned CallSiteID) {
  atomic_inc(&LoomCounter[CallSiteID]);
  pthread_rwlock_unlock(&LoomUpdateLock);
}

extern "C" void LoomAfterBlocking(unsigned CallSiteID) {
  pthread_rwlock_rdlock(&LoomUpdateLock);
  atomic_dec(&LoomCounter[CallSiteID]);
}

extern "C" void LoomEnterThread() {
  pthread_rwlock_rdlock(&LoomUpdateLock);
}

extern "C" void LoomExitThread() {
  pthread_rwlock_unlock(&LoomUpdateLock);
}

// LoomEnterProcess is called before all global_ctors including the constructor
// of iostream stuff. Make it pure C.
// Similar rules go to LoomExitProcess.
extern "C" void LoomEnterProcess() {
  fprintf(stderr, "***** LoomEnterProcess *****\n");
  pthread_rwlock_init(&LoomUpdateLock, NULL);
  memset((void *)LoomWait, 0, sizeof(LoomWait));
  memset((void *)LoomCounter, 0, sizeof(LoomCounter));
  LoomEnterThread();
}

extern "C" void LoomExitProcess() {
  fprintf(stderr, "***** LoomExitProcess *****\n");
  LoomExitThread();
}

void EvacuateAndUpdate(const set<int> &UnsafeBackEdges,
                       const set<int> &UnsafeCallSites) {
  // Turn on wait flags for all safe back edges.
  for (int i = 0; i < MaxNumBackEdges; ++i) {
    if (!UnsafeBackEdges.count(i))
      LoomWait[i] = true;
  }

  // Make sure nobody is running inside an unsafe call site.
  while (true) {
    pthread_rwlock_wrlock(&LoomUpdateLock);
    bool InBlockingCallSite = false;
    for (set<int>::const_iterator I = UnsafeCallSites.begin();
         I != UnsafeCallSites.end();
         ++I) {
      if (LoomCounter[*I] > 0) {
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
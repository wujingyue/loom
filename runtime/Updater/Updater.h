#ifndef __LOOM_UPDATER_H
#define __LOOM_UPDATER_H

#include <pthread.h>

#include "Sync.h"
#include "Operation.h"

static const unsigned MaxNumBackEdges = 65536;
static const unsigned MaxNumBlockingCS = 65536;
static const unsigned MaxNumInsts = 2000000;

// evacuation algorithm
extern volatile bool LoomWait[MaxNumBackEdges];
extern atomic_t LoomCounter[MaxNumBlockingCS];
extern pthread_rwlock_t LoomUpdateLock;
// slot operations
extern Operation *LoomOperations[MaxNumInsts];

int StartDaemon();
int StopDaemon();

#endif

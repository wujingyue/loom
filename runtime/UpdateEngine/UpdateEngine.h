#ifndef __LOOM_UPDATER_H
#define __LOOM_UPDATER_H

#include <pthread.h>

#include "Sync.h"
#include "Operation.h"

#define MaxNumBackEdges (65536)
#define MaxNumBlockingCS (65536)
#define MaxNumInsts (2000000)

// evacuation algorithm
extern volatile int LoomWait[MaxNumBackEdges];
extern atomic_t LoomCounter[MaxNumBlockingCS];
extern pthread_rwlock_t LoomUpdateLock;
// slot operations
extern struct Operation *LoomOperations[MaxNumInsts];

int StartDaemon();
int StopDaemon();

int AddFix(int FixID, const char *FileName);
int DeleteFix(int FixID);

#endif

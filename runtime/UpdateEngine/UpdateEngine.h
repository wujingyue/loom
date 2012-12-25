#ifndef __LOOM_UPDATER_H
#define __LOOM_UPDATER_H

#include <pthread.h>

#include "Sync.h"
#include "loom/config.h"

typedef void *ArgumentType;
typedef void (*CallBackType)(ArgumentType);

struct Operation {
  CallBackType CallBack;
  ArgumentType Arg;
  unsigned SlotID;
  struct Operation *Next;
};

/* control application threads */
extern volatile int LoomWait[MaxNumBackEdges];
extern atomic_t LoomCounter[MaxNumBlockingCS];
extern pthread_rwlock_t LoomUpdateLock;
/*
 * LoomOperations[i] points to the first operation in slot i. Other operations
 * are chained via the Next pointer in struct Operation.
 */
extern int LoomSwitches[MaxNumFuncs];
extern struct Operation *LoomOperations[MaxNumInsts];
extern pthread_mutex_t Mutexes[MaxNumFilters];

void PrependOperation(struct Operation *Op, struct Operation **Pos);
int UnlinkOperation(struct Operation *Op, struct Operation **List);

int StartDaemon();
int StopDaemon();

void InitFilters();
void ClearFilters();

#endif

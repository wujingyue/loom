#ifndef __LOOM_UPDATER_H
#define __LOOM_UPDATER_H

#include <pthread.h>

#include "Sync.h"

#define MaxNumBackEdges (65536)
#define MaxNumBlockingCS (65536)
#define MaxNumInsts (2000000)
#define MaxNumFilters (1024)

typedef void *ArgumentType;
typedef void (*CallBackType)(ArgumentType);

struct Operation {
  CallBackType CallBack;
  ArgumentType Arg;
  unsigned SlotID;
  struct Operation *Next;
};

// control application threads
extern volatile int LoomWait[MaxNumBackEdges];
extern atomic_t LoomCounter[MaxNumBlockingCS];
extern pthread_rwlock_t LoomUpdateLock;
// slot operations
extern struct Operation *LoomOperations[MaxNumInsts];
extern pthread_mutex_t Mutexes[MaxNumFilters];

void PrependOperation(struct Operation *Op, struct Operation **Pos);
int UnlinkOperation(struct Operation *Op, struct Operation **List);
void ClearOperations(struct Operation **Op);

int StartDaemon();
int StopDaemon();

void InitFilters();

#endif

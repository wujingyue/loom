#ifndef __LOOM_UPDATER_H
#define __LOOM_UPDATER_H

#include <pthread.h>

#include "Sync.h"

static const unsigned MaxNumBackEdges = 65536;
static const unsigned MaxNumBlockingCS = 65536;
static const unsigned MaxNumInsts = 2000000;

volatile bool LoomWait[MaxNumBackEdges];
volatile atomic_t LoomCounter[MaxNumBlockingCS];
pthread_rwlock_t LoomUpdateLock;

#endif

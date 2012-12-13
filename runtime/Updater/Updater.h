#ifndef __LOOM_UPDATER_H
#define __LOOM_UPDATER_H

#include <pthread.h>

#include "Sync.h"

static const int MaxNumBackEdges = 65536;
static const int MaxNumBlockingCS = 65536;

volatile bool LoomWait[MaxNumBackEdges];
volatile atomic_t LoomCounter[MaxNumBlockingCS];
pthread_rwlock_t LoomUpdateLock;

#endif

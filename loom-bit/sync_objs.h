#ifndef __SYNC_OBJS_H
#define __SYNC_OBJS_H

#include "loom.h"

#define MAX_N_FIXES (10000)

void init_sync_objs();
int enter_critical_region(argument_t arg);
int exit_critical_region(argument_t arg);
int enter_atomic_region(argument_t arg);
int exit_atomic_region(argument_t arg);
int semaphore_down(argument_t arg);
int semaphore_up(argument_t arg);

#endif


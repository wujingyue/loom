#ifndef __LOOM_SYNC_H
#define __LOOM_SYNC_H

typedef volatile unsigned atomic_t;

static inline int atomic_dec(atomic_t *v) {
  return __sync_sub_and_fetch(v, 1);
}

static inline int atomic_inc(atomic_t *v) {
  return __sync_add_and_fetch(v, 1);
}

void EnterCriticalRegion(void *Arg);
void ExitCriticalRegion(void *Arg);

#endif

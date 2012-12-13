#include "Updater.h"

extern "C" void LoomInit() {
  pthread_rwlock_init(&LoomUpdateLock, NULL);
}

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

extern "C" void LoomEnterThread(unsigned) {
  pthread_rwlock_rdlock(&LoomUpdateLock);
}

extern "C" void LoomExitThread(unsigned) {
  pthread_rwlock_unlock(&LoomUpdateLock);
}

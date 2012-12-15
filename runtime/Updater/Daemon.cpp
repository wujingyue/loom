#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <signal.h>
#include <unistd.h>
#include <sys/prctl.h>

#include "Updater.h"

static pthread_t DaemonTID;

int BlockAllSignals() {
  sigset_t SigSet;
  if (sigfillset(&SigSet) == -1) {
    perror("sigfillset");
    return -1;
  }

  if (pthread_sigmask(SIG_BLOCK, &SigSet, NULL) != 0) {
    perror("pthread_sigmask");
    return -1;
  }

  return 0;
}

void SetThreadName() {
  if (prctl(PR_SET_NAME, "loom-daemon", 0, 0, 0) == -1) {
    perror("prctl");
  }
}

void *RunDaemon(void *) {
  // Block all signals. Applications such as MySQL and Apache have their own way
  // of handling signals, which we do not want to interfere. For instance, MySQL
  // has a special signal handling thread, which calls sigwait to wait for
  // signals. If the Loom daemon stole the signal, the sigwait would never
  // return, and the server would not be killed.
  if (BlockAllSignals() == -1)
    return (void *)-1;

  // Set the thread name, so that we can "ps c" to view it.
  SetThreadName();

  while (1) {
    fprintf(stderr, "daemon is running...\n");
    sleep(1);
  }

  // unreachable
  assert(false);

  return NULL;
}

int StartDaemon() {
  if (pthread_create(&DaemonTID, NULL, RunDaemon, NULL) == -1) {
    perror("pthread_create");
    return -1;
  }

	fprintf(stderr, "Daemon TID = %lu\n", DaemonTID);
	return 0;
}

int StopDaemon() {
  fprintf(stderr, "StopDaemon\n");
  // The daemon thread will be automatically killed by the parent process. No
  // need to explicitly kill it.
  return 0;
}

#include <cstdio>
#include <cstdlib>
#include <signal.h>
#include <unistd.h>
#include <sys/prctl.h>

#include "Updater.h"

static pthread_t DaemonTID;

void *RunDaemon(void *) {
  // set the thread name
  // We can "ps c" to view it.
  if (prctl(PR_SET_NAME, "loom-daemon", 0, 0, 0) == -1) {
    perror("prctl");
  }
  while (1) {
    fprintf(stderr, "daemon is running...\n");
    sleep(1);
  }
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

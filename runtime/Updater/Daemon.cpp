#include <cstdio>
#include <cstdlib>
#include <signal.h>
#include <unistd.h>
#include <sys/prctl.h>

#include "Updater.h"

static int DaemonPID = -1;

int RunDaemon(void *) {
  signal(SIGTERM, SIG_DFL);
  if (prctl(PR_SET_NAME, "loom-daemon", 0, 0, 0) == -1) {
    perror("prctl");
  }
  while (1) {
    fprintf(stderr, "daemon is running...\n");
    sleep(1);
  }
  return 0;
}

int StartDaemon() {
	static const int ChildStackSize = 20 * 1024 * 1024;
	char *ChildStack = (char *)malloc(ChildStackSize);
	if (!ChildStack) {
		perror("malloc");
		return -1;
	}

	DaemonPID = clone(RunDaemon, ChildStack + ChildStackSize, CLONE_VM, NULL);
  if (DaemonPID == -1) {
    perror("clone");
    return -1;
  }

	fprintf(stderr, "Daemon PID = %d\n", DaemonPID);
	return 0;
}

int StopDaemon() {
  fprintf(stderr, "StopDaemon\n");
  if (DaemonPID != -1) {
    fprintf(stderr, "Kill %d\n", DaemonPID);
    if (kill(DaemonPID, SIGTERM) == -1) {
      perror("kill");
      return -1;
    }
  }
  return 0;
}

#include <cstdio>
#include <cstdlib>
#include <signal.h>
#include <unistd.h>

#include "Updater.h"

static int DaemonPID = -1;

int RunDaemon(void *Arg) {
  while (1) {
    printf("daemon is running...\n");
    sleep(1);
  }
  return 0;
}

int StartDaemon() {
	static const int ChildStackSize = 1024 * 1024;
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
  if (DaemonPID != -1) {
    if (kill(DaemonPID, SIGTERM) == -1) {
      perror("kill");
      return -1;
    }
  }
  return 0;
}

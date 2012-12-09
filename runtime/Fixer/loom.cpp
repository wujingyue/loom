#define _REENTRANT

#include <cassert>
#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <signal.h>
#include <sys/prctl.h>
#include <pthread.h>
#include <sys/time.h>

#include "loom/sync.h"
#include "loom/loom.h"
#include "loom/daemon.h"
#include "loom/fixes.h"

using namespace std;

spin_rwlock_t updating_nthreads;
volatile int in_atomic_region;
atomic_t executing_basic_func[MAX_N_CHECKS];
atomic_t in_check[MAX_N_CHECKS];

volatile int is_func_patched[MAX_N_FUNCS];
volatile int enabled[MAX_N_CHECKS];
// TODO: multiple callback functions
volatile inject_func_t callback[MAX_N_INSTS];
argument_t arguments[MAX_N_INSTS];

int daemon_pid = -1;

void stop_daemon() {
#if 0
	char file_name[1024];
	sprintf(file_name, "/tmp/log.%d", getpid());
	flog = fopen(file_name, "a");
	fprintf(flog, "stop_daemon %d\n", daemon_pid);
	fclose(flog);
#endif
	if (daemon_pid != -1)
		kill(daemon_pid, 9);
}

int start_daemon() {
	const static int CHILD_STACK_SIZE = 1024 * 1024;
	void *child_stack = (char *)malloc(CHILD_STACK_SIZE) + CHILD_STACK_SIZE;
	if (!child_stack) {
		perror("malloc");
		return -1;
	}
	daemon_pid = clone(handle_client_requests, child_stack, CLONE_VM, NULL);
	fprintf(stderr, "daemon_pid = %d\n", daemon_pid);
	sleep(3);
	return 0;
}

// #define MEASURE_TIME

/*
 * Returns -1 on error. But still need to activate.
 */
int deactivate(const vector<int> &checks, int &n_tries) {
#if 0
	fprintf(stderr, "checks:");
	for (size_t i = 0; i < checks.size(); ++i)
		fprintf(stderr, " %d", checks[i]);
	fprintf(stderr, "\n");
#endif
	int tmp[MAX_N_CHECKS];
#ifdef MEASURE_TIME
	struct timeval start, t1, t2, t3, diff;
	gettimeofday(&start, NULL);
#endif

	for (int i = 0; i < MAX_N_CHECKS; ++i)
		tmp[i] = 1;
	for (size_t i = 0; i < checks.size(); ++i) {
		int check_id = checks[i];
		tmp[check_id] = 0;
	}
	for (int i = 0; i < MAX_N_CHECKS; ++i) {
		if (tmp[i] == 1)
			enabled[i] = 1;
	}

#ifdef MEASURE_TIME
	gettimeofday(&t1, NULL);
	timersub(&t1, &start, &diff);
	FILE *flog = fopen("/tmp/log", "a");
	fprintf(flog, "setting flag = %ld\n", diff.tv_sec * 1000000 + diff.tv_usec);
	fclose(flog);
#endif

	fprintf(stderr, "Deactivating...\n");
	n_tries = 0;
#if 1
	while (true) {
		n_tries++;
		spin_write_lock(&updating_nthreads);
#ifdef MEASURE_TIME
		if (n_tries == 1) {
			gettimeofday(&t2, NULL);
			timersub(&t2, &t1, &diff);
			flog = fopen("/tmp/log", "a");
			fprintf(flog, "grabbing lock = %ld\n", diff.tv_sec * 1000000 + diff.tv_usec);
			fclose(flog);
		}
#endif
		// fprintf(stderr, "Grabbed the global lock\n");
		bool ok = true;
		for (int i = 0; i < MAX_N_CHECKS; ++i) {
			if (enabled[i] == 0 && (executing_basic_func[i] > 0 || in_check[i] > 0)) {
				// fprintf(stderr, "check %d does not satisfy\n", i);
				ok = false;
				break;
			}
		}
		if (ok)
			break;
		spin_write_unlock(&updating_nthreads);
		usleep(10 * 1000);
	}
#else
	n_tries++;
	spin_write_lock(&updating_nthreads);
	for (int i = 0; i < MAX_N_CHECKS; ++i) {
		if (enabled[i] == 0 && (executing_basic_func[i] > 0 || in_check[i] > 0)) {
			// fprintf(stderr, "check %d does not satisfy\n", i);
			return -1;
		}
	}
#endif

#ifdef MEASURE_TIME
	gettimeofday(&t3, NULL);
	timersub(&t3, &t2, &diff);
	flog = fopen("/tmp/log", "a");
	fprintf(flog, "waiting = %ld\n", diff.tv_sec * 1000000 + diff.tv_usec);
	fclose(flog);
#endif
	return 0;
}

int activate() {
	int i;
	int ok_to_activate;

	spin_write_unlock(&updating_nthreads);
	for (i = 0; i < MAX_N_CHECKS; ++i)
		enabled[i] = 0;
	do {
		ok_to_activate = 1;
		for (i = 0; i < MAX_N_CHECKS; ++i) {
			if (in_check[i] > 0) {
				ok_to_activate = 0;
				break;
			}
		}
	} while (ok_to_activate == 0);
	return 0;
}

// #define SAMPLING_ENABLED

#ifdef SAMPLING_ENABLED
static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
#endif

int __enter_atomic_region() {
	for (int i = 0; i < MAX_N_CHECKS; ++i)
		enabled[i] = 1;
#ifdef SAMPLING_ENABLED
	pthread_mutex_lock(&m);
	/*
	FILE *fout = fopen("/tmp/log", "a");
	fprintf(fout, "Entering the atomic region...\n");
	fclose(fout);
	*/
	fprintf(stderr, "Entering the atomic region...\n");
	pthread_mutex_unlock(&m);
#endif
	spin_read_unlock(&updating_nthreads);
	spin_write_lock(&updating_nthreads);
	// fprintf(stderr, "Grabbed the global lock\n");
	in_atomic_region = 1;
	for (int i = 0; i < MAX_N_CHECKS; ++i)
		enabled[i] = 0;
	return 0;
}

int __exit_atomic_region() {
	int i, ok_to_activate;
#ifdef SAMPLING_ENABLED
	pthread_mutex_lock(&m);
	/*
	FILE *fout = fopen("/tmp/log", "a");
	fprintf(fout, "Exiting the atomic region...\n");
	fclose(fout);
	*/
	fprintf(stderr, "Exiting the atomic region...\n");
	pthread_mutex_unlock(&m);
#endif
	in_atomic_region = 0;
	spin_write_unlock(&updating_nthreads);
	do {
		ok_to_activate = 1;
		for (i = 0; i < MAX_N_CHECKS; ++i) {
			if (in_check[i] > 0) {
				ok_to_activate = 0;
				break;
			}
		}
	} while (ok_to_activate == 0);
	spin_read_lock(&updating_nthreads);
	return 0;
}

#ifndef OPT3
extern "C" int loom_before_call(int check_id) {
#ifdef SAMPLING_ENABLED
	static int counter = 0;
	counter++;
	if (counter >= 1000) {
		pthread_mutex_lock(&m);
		FILE *fout = fopen("/tmp/log", "a");
		fprintf(fout, "before_call: %d\n", check_id);
		fclose(fout);
		pthread_mutex_unlock(&m);
		counter = 0;
	}
#endif
#ifdef EXTERN_FUNC_ENABLED
	if (in_atomic_region)
		return 1;
	atomic_inc(&executing_basic_func[check_id]);
	spin_read_unlock(&updating_nthreads);
#endif
	return 0;
}

extern "C" void loom_after_call(int check_id) {
#ifdef SAMPLING_ENABLED
	static int counter = 0;
	counter++;
	if (counter >= 1000) {
		pthread_mutex_lock(&m);
		FILE *fout = fopen("/tmp/log", "a");
		fprintf(fout, "before_call: %d\n", check_id);
		fclose(fout);
		pthread_mutex_unlock(&m);
		counter = 0;
	}
#endif
#ifdef EXTERN_FUNC_ENABLED

#ifdef WAIT_CHECK
	while (unlikely(enabled[check_id]));
#endif
	spin_read_lock(&updating_nthreads);
	atomic_dec(&executing_basic_func[check_id]);
#endif
}

#endif // OPT3

#ifndef INLINE_DISPATCHER
extern "C" void loom_dispatcher(int ins_id) {
	if (likely(callback[ins_id] == NULL))
		return;
	inject_func_t func = callback[ins_id];
	func(arguments[ins_id]); 
}
#endif


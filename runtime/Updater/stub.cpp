#define _REENTRANT

// #define SAMPLING_ENABLED
#define EXTERN_FUNC_ENABLED
#define BACK_EDGE_ENABLED
//#define SKIP_SPECIFIED_BACK_EDGES // TODO: merge into OPT 5
#define SWITCH_ENABLED
#define WAIT_CHECK

#include <iostream>
#include <cassert>
#include <cstring>
#include <cstdio>
#include <cstdlib>

#include <signal.h>
#include <sys/prctl.h>
#include <pthread.h>

#include "loom/loom.h"
#include "loom/fixes.h"

using namespace std;

extern "C" __attribute__((always_inline)) int loom_is_func_patched(int func_id) {
#ifdef SWITCH_ENABLED
	return unlikely(is_func_patched[func_id]);
#else
	return 0;
#endif
}

#ifdef SAMPLING_ENABLED
static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
#endif

#ifdef OPT3

extern "C" __attribute__((always_inline)) int loom_before_call(int check_id) {
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

extern "C" __attribute__((always_inline)) void loom_after_call(int check_id) {
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

__attribute__((noinline)) void __loom_check(int check_id) {
	atomic_inc(&in_check[check_id]);
	spin_read_unlock(&updating_nthreads);
	while (enabled[check_id]);
	spin_read_lock(&updating_nthreads);
	atomic_dec(&in_check[check_id]);
}

extern "C" __attribute__((always_inline)) void loom_check(int check_id) {
#ifdef SKIP_SPECIFIED_BACK_EDGES
	if (check_id == 1867)
		return;
	if (check_id == 32701)
		return;
#endif

#ifdef SAMPLING_ENABLED
	static int counter = 0;
	counter++;
	if (counter >= 1000) {
		pthread_mutex_lock(&m);
		FILE *fout = fopen("/tmp/log", "a");
		fprintf(fout, "check: %d\n", check_id);
		fclose(fout);
		pthread_mutex_unlock(&m);
		counter = 0;
	}
#endif

#ifdef BACK_EDGE_ENABLED

#ifdef WAIT_CHECK
	/* OPT 2: Double checking locking on enabled */
	if (unlikely(enabled[check_id]))
		__loom_check(check_id);
#else
	atomic_inc(&in_check[check_id]);
	spin_read_unlock(&updating_nthreads);
	spin_read_lock(&updating_nthreads);
	atomic_dec(&in_check[check_id]);
#endif

#endif
}

extern "C" void loom_enter_thread_func(int check_id) {
	spin_read_lock(&updating_nthreads);
}

extern "C" void loom_exit_thread_func(int check_id) {
	spin_read_unlock(&updating_nthreads);
}

extern "C" void loom_fini() {
	char name[1024];
	int ret = prctl(PR_GET_NAME, name, 0, 0, 0);
	assert(ret == 0);
	if (strcmp(name, "daemon") == 0)
		return;

	loom_exit_thread_func(-2);
	stop_daemon();
}

static void loom_init() {
	char name[1024];
	int ret = prctl(PR_GET_NAME, name, 0, 0, 0);
	assert(ret == 0);
	if (strcmp(name, "daemon") == 0)
		return;

	fprintf(stderr, "************** loom_init ****************\n");

	start_daemon();
}

extern "C" void loom_setup() {
	int i;
	for (i = 0; i < MAX_N_CHECKS; ++i) {
		executing_basic_func[i] = 0;
		in_check[i] = 0;
		enabled[i] = 0;
	}
	spin_rwlock_init(&updating_nthreads);
	in_atomic_region = 0;
	for (i = 0; i < MAX_N_FUNCS; i++)
		is_func_patched[i] = 0;
	for (i = 0; i < MAX_N_INSTS; i++) {
		callback[i] = NULL;
		arguments[i] = NULL;
	}
	init_fixes();
	init_sync_objs();
	loom_enter_thread_func(-2);

#if 0
	char err_msg[1024];
	if (preload_fix(0, "/home/jingyue/Research/defens-new/loom-bit/tests/fft-order.fix", err_msg) < 0) {
		fprintf(stderr, "error when preloading fix\n");
		return;
	}
#endif

	/*
	 * Register fork handler and exit handler. 
	 * These handlers will be inherited by forked processes,
	 * thus cannot be put in loom_init.
	 */
	if (pthread_atfork(NULL, NULL, loom_init) == -1) {
		perror("pthread_atfork");
		return;
	}
	if (atexit(loom_fini) == -1) {
		perror("atexit");
		return;
	}
	loom_init();
}

#ifdef INLINE_DISPATCHER

extern "C" void loom_dispatcher(int ins_id) {
	if (likely(callback[ins_id] == NULL))
		return;
	inject_func_t func = callback[ins_id];
	func(arguments[ins_id]); 
}

#endif


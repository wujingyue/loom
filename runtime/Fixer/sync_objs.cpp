#define _REENTRANT

#include <cstdio>
#include <pthread.h>
#include <semaphore.h>
using namespace std;

#include "loom/sync_objs.h"
#include "loom/loom.h"

static pthread_mutex_t mutexes[MAX_N_FIXES];
static sem_t sems[MAX_N_FIXES];

void init_sync_objs() {
	for (int i = 0; i < MAX_N_FIXES; ++i) {
		pthread_mutex_init(&mutexes[i], NULL);
		sem_init(&sems[i], 0, 0);
	}
}

int enter_critical_region(argument_t arg) {
	long fix_id = (long)arg;
	pthread_mutex_lock(&mutexes[fix_id]);
	// fprintf(stderr, "enter_critical_region %p\n", &mutexes[fix_id]);
	return 0;
}

int exit_critical_region(argument_t arg) {
	long fix_id = (long)arg;
	// fprintf(stderr, "exit_critical_region %p\n", &mutexes[fix_id]);
	pthread_mutex_unlock(&mutexes[fix_id]);
	return 0;
}

int enter_atomic_region(argument_t arg) {
	return __enter_atomic_region();
}

int exit_atomic_region(argument_t arg) {
	return __exit_atomic_region();
}

int semaphore_up(argument_t arg) {
	fprintf(stderr, "semaphore_up\n");
	long fix_id = (long)arg;
	sem_post(&sems[fix_id]);
	return 0;
}

int semaphore_down(argument_t arg) {
	fprintf(stderr, "semaphore_down\n");
	long fix_id = (long)arg;
	sem_wait(&sems[fix_id]);
	return 0;
}


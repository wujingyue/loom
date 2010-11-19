#include <iostream>
#include <pthread.h>

using namespace std;

const int N_ITERS = 100;

static volatile int global = 0;

void *func(void *arg) {
	for (int i = 0; i < N_ITERS; i++) {
		int t = global;
		usleep(10 * 1000);
		t++;
		global = t;
	}
}

int main() {
	pthread_t t1, t2;
	pthread_create(&t1, 0, func, 0);
	pthread_create(&t2, 0, func, 0);
	pthread_join(t1, 0);
	pthread_join(t2, 0);
	cerr << "global = " << global << endl;
	if (global == N_ITERS * 2)
		cerr << "OK" << endl;
	else
		cerr << "Race occured" << endl;
	return 0;
}


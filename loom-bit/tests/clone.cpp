#include <iostream>
#include <pthread.h>
#include <cstring>
using namespace std;

void *thread_func(void *arg) {
	sleep(2);
	pthread_exit(0);
}

int main(int argc, char *argv[]) {
	int counter = 0;
	for (int i = 0; i < argc; i++)
		counter += strlen(argv[i]);
	cerr << counter << endl;

	pthread_t t1, t2;
	while (1) {
		fprintf(stderr, "iteration\n");
		pthread_create(&t1, 0, thread_func, 0);
		pthread_create(&t2, 0, thread_func, 0);
		pthread_join(t1, 0);
		pthread_join(t2, 0);
	}
	return 0;
}


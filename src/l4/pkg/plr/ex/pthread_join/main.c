#include <stdio.h>
#include <unistd.h>
#include <assert.h>

#include <pthread.h>
#include <pthread-l4.h>

#include <l4/re/env.h>
#include <l4/sys/kdebug.h>

static
void *thread(void *data)
{
	(void)data;
	for (unsigned i = 0; i < 2; ++i) {
	//while (1) {
		printf("\033[31mhello world from thread\033[0m\n");
		sleep(1);
	}
	return NULL;
}

int main(int argc, char **argv)
{
	(void)argc; (void)argv;

	pthread_t pt;
	int res;

	printf("\033[32mhello from main thread\033[0m\n");

	for (unsigned i = 0; i < 5; ++i) {
		printf("\033[32mLaunching worker thread.\033[0m\n");
		res = pthread_create(&pt, NULL, thread, NULL);
		assert(res == 0);
		res = pthread_join(pt, NULL);
		printf("\033[32mWorker thread returned.\033[0m\n");
		assert(res == 0);
	}

	return 0;
}

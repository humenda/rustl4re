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
	for (unsigned i = 0; i < 5; ++i) {
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
	printf("\033[32mhello from main thread\033[0m\n");

	int res = pthread_create(&pt, NULL, thread, NULL);
	assert(res == 0);

	for (unsigned i = 0; i < 5; ++i) {
	//while (1) {
		printf("\033[32mhello world from main\033[0m\n");
		sleep(1);
	}

	pthread_join(pt, NULL);

	enter_kdebug("before return");

	return 0;
}

#include <stdio.h>

#include <l4/util/util.h>
#include <l4/util/rdtsc.h>

#include <l4/plr/measurements.h>

int main(void)
{
	printf("starting Hello (C version)\n");

	void *ebuf = (void*)0xB0000000;
	
	unsigned i = 0;
	for ( ; i < 5; ++i) {

		struct GenericEvent *ev = evbuf_next(ebuf);
		ev->header.tsc = evbuf_get_time(ebuf, 1);
		ev->header.vcpu = 0x1234;
		ev->header.type = 4;

		printf("HELLO..\n");

		l4_sleep(1000);
	}
	return 0;
}

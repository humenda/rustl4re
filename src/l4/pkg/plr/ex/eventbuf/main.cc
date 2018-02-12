/*
 * eventbuf/main.cc --
 *
 * Test for the shared-memory event buffer
 *
 * (c) 2013 Björn Döbel <doebel@os.inf.tu-dresden.de>,
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <cstdlib>
#include <cstdio>
#include <cstring>

#include <l4/util/rdtsc.h>
#include <l4/util/util.h>

#include <l4/plr/uu.h>
#include <l4/plr/measurements.h>

Measurements::EventBuf eb;

unsigned char buffer[1024 * 1024];

int main()
{
	printf("EB\n");
	eb.set_buffer(buffer, sizeof(buffer));
	printf("EB.buf = %p, size %ld, index %d\n",
	       eb.buffer, eb.size, eb.index);

	printf("Generating 200 events...\n");
	for (unsigned i = 0; i < 100; ++i) {

		Measurements::GenericEvent* ev = eb.next();
		ev->header.tsc   = l4_rdtsc();
		ev->header.vcpu  = 0x1234;
		ev->header.type  = Measurements::Foo;
		ev->data.foo.start = 1;
		printf("%d %llx\n", ev->header.type, ev->header.tsc);
		
		l4_sleep(random() % 100);
		
		ev = eb.next();
		ev->header.tsc   = l4_rdtsc();
		ev->header.vcpu  = 0x1234;
		ev->header.type  = Measurements::Foo;
		ev->data.foo.start = 0;
	}

	printf("EB.buf = %p, size %ld, index %d\n",
	       eb.buffer, eb.size, eb.index);
	printf("EB.oldest = %ld\n", eb.oldest());

	char const *filename = "sampledump.txt";

	unsigned oldest = eb.oldest();
	unsigned dump_start, dump_size;

	if (oldest == 0) { // half-full -> dump from 0 to index
		dump_start = 0;
		dump_size  = eb.index * sizeof(Measurements::GenericEvent);
	} else { // buffer completely full -> dump full size starting from oldest entry
		dump_start = oldest * sizeof(Measurements::GenericEvent);
		dump_size  = eb.size * sizeof(Measurements::GenericEvent);
	}

	uu_dumpz_ringbuffer(filename, eb.buffer, eb.size * sizeof(Measurements::GenericEvent),
	                    dump_start, dump_size);
	return 0;
}

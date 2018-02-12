/*
 * large_malloc/main.cc --
 * 
 *    Try out a couple of malloc/l4_touch_rw/free sequences
 *    to figure out the overhead for Romain's memory management.
 *
 * (c) 2013 Björn Döbel <doebel@os.inf.tu-dresden.de>,
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>

#include <l4/util/util.h>
#include <l4/util/reboot.h>
#include <l4/util/rdtsc.h>
#include <l4/re/env>
#include <l4/re/mem_alloc>
#include <l4/re/rm>
#include <l4/re/util/cap_alloc>
#include <l4/sys/kdebug.h>

#include "log"

#define NUM_SIZES 5

/*
 */
static inline long long US(struct timeval& tv)
{
	return tv.tv_sec * 1000000 + tv.tv_usec;
}

static void
__attribute__((used))
malloc_test()
{
	printf("Malloc Benchmark\n");
	l4_umword_t sizes[NUM_SIZES] = {
		      1024 * 1024, // 1 MB
		 64 * 1024 * 1024, // 64 MB
		100 * 1024 * 1024, // 100 MB
		300 * 1024 * 1024, // 300 MB
		500 * 1024 * 1024, // 500 MB
	};

	for (unsigned i = 0; i < NUM_SIZES; ++i) {
		struct timeval tstart, tmalloc, ttouch, tfree;

		printf("%s==== %d -> %lx ====%s\n", BOLD_CYAN, i, sizes[i], NOCOLOR);

		gettimeofday(&tstart, 0);
		char *p = reinterpret_cast<char*>(malloc(sizes[i]));
		printf("malloc(%lx) = %p (%d)\n",
		       sizes[i], p, errno);
		if (!p)
			enter_kdebug("malloc");
		gettimeofday(&tmalloc, 0);
		l4_touch_rw(p, sizes[i]);
		gettimeofday(&ttouch, 0);
		printf("... touched.\n");
		free(p);
		gettimeofday(&tfree, 0);
		printf("%sfreed.%s\n", BOLD_GREEN, NOCOLOR);

		printf("malloc %10lld µs, touch %10lld µs, free %10lld µs\n",
		       US(tmalloc) - US(tstart),
		       US(ttouch)  - US(tmalloc),
		       US(tfree)   - US(ttouch));
	}
}


static unsigned long long rdtsc1()
{
	unsigned long long ret = 0;
	unsigned long hi, lo;
	asm volatile ("cpuid\t\n"
				  "rdtsc\t\n"
				  "mov %%edx, %0\n\t"
				  "mov %%eax, %1\n\t"
				  : "=r"(hi), "=r"(lo)
				  :
				  : "eax", "ebx", "ecx", "edx");
	ret = hi;
	ret <<= 32;
	ret |= lo;
	return ret;
}


static unsigned long long rdtsc2()
{
	unsigned long long ret = 0;
	unsigned long hi, lo;
	asm volatile ("rdtscp\n\t"
				  "mov %%edx, %0\n\t"
				  "mov %%eax, %1\n\t"
				  "cpuid\n\t"
				  : "=r"(hi), "=r"(lo)
				  :
				  : "eax", "ebx", "ecx", "edx");
	ret = hi;
	ret <<= 32;
	ret |= lo;
	//printf("%lx %lx %llx\n", hi, lo, ret);
	return ret;
}


static void rm_test()
{
	unsigned size = 1024 * 1024 * 1024; // 1 GB
	unsigned flags = 0; // L4Re::Mem_alloc::Super_pages | L4Re::Mem_alloc::Continuous;
	unsigned long long alloc1, alloc2, attach1, attach2, touch1, touch2;

	printf("RM Benchmark\n");

	alloc1 = rdtsc1();

	//printf("Start RM test. %x\n", flags);

	L4::Cap<L4Re::Dataspace> dscap = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();
	L4::Cap<L4Re::Dataspace> dscap2;
	//printf("DS: %lx\n", dscap.cap());
	int error = L4Re::Env::env()->mem_alloc()->alloc(size, dscap, flags);
	if (error) {
		enter_kdebug("alloc fail");
	}
	alloc2 = rdtsc2();

	attach1 = rdtsc1();
	l4_addr_t a = 0;
	error = L4Re::Env::env()->rm()->attach(&a, size, L4Re::Rm::Search_addr /* | L4Re::Rm::Eager_map*/, dscap,
										   0, L4_SUPERPAGESHIFT);
	//printf("Attached to %p\n", (void*)a);
	attach2 = rdtsc2();

	touch1 = rdtsc1();
	l4_touch_rw((void*)a, size);
	touch2 = rdtsc2();

	//L4Re::Env::env()->rm()->detach(a, &dscap2);

	printf("mem_alloc: %lld\n", alloc2 - alloc1);
	printf("attach   : %lld\n", attach2 - attach1);
	printf("touch_rw : %lld\n", touch2 - touch1);

	//printf("End RM test.\n");
}


static void access_test()
{
	unsigned size = 1024 * 1024 * 1024; // 1 GB
	unsigned flags = 0;
	unsigned long long set1, set2, cp1, cp2, acc1, acc2;
	enum { ITERATIONS = 50,
	       RAND_ITER  = 100000,
	       RAND_REPEAT = 100,
	       CLOCK = 3400 };

	printf("Mem Access Benchmark\n");

	L4::Cap<L4Re::Dataspace> dscap = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();
	L4::Cap<L4Re::Dataspace> dscap2;
	//printf("DS: %lx\n", dscap.cap());
	int error = L4Re::Env::env()->mem_alloc()->alloc(size, dscap, flags);
	if (error) {
		enter_kdebug("alloc fail");
	}

	l4_addr_t a = 0;
	error = L4Re::Env::env()->rm()->attach(&a, size, L4Re::Rm::Search_addr /* | L4Re::Rm::Eager_map*/, dscap,
	                                       0, L4_SUPERPAGESHIFT);

	l4_touch_rw((void*)a, size);

	/* Memory is now attached and paged in. */

#if 0
	// tell our parent about the shared area address
	asm volatile ("int $0x42\t\n"
				  :
				  : "A" (a)
	);
#endif

	printf("memset...\n");

	set1 = rdtsc1();
	for (unsigned i = 0; i < ITERATIONS; ++i) {
		memset((void*)a, 0xAB, size);
	}

	set2 = rdtsc2();

	// setup memcpy
	unsigned size2 = size >> 2;
	l4_addr_t dest = a + size2;

	printf("memcpy...\n");

	cp1 = rdtsc1();
	for (unsigned i = 0; i < ITERATIONS; ++i) {
		memcpy((void*)dest, (void*)a, size2);
	}
	cp2 = rdtsc2();

	// setup random addresses
	static l4_addr_t dests[RAND_ITER];
	for (unsigned i = 0; i < RAND_ITER; ++i) {
		dests[i] = a + (random() % size);
	}
	printf("random access...\n");

	acc1 = rdtsc1();
	for (unsigned j = 0; j <= RAND_REPEAT; ++j) {
		for (unsigned i = 0; i < RAND_ITER; ++i) {
			*(unsigned*)dests[i] = 0xCD;
		}
	}
	acc2 = rdtsc2();

	printf("%llx %llx\n", set1, set2);
	printf("%llx %llx\n", cp1, cp2);
	printf("%llx %llx\n", acc1, acc2);
	printf("check: %p %p %p\n", (void*)dests[25],
		   (void*)dests[42], (void*)dests[2555]);

	unsigned long long diff = set2 - set1;
	printf("memset: %16lld cyc (%16lld / iteration)\n"
		   "        %16lld us\n",
		   diff, diff/ITERATIONS,
		   diff / CLOCK);

	diff = cp2 - cp1;
	printf("memcpy: %16lld cyc (%16lld / iteration)\n"
		   "        %16lld us\n",
		   diff, diff/ITERATIONS,
		   diff / CLOCK);

	diff = acc2 - acc1;
	printf("random: %16lld cyc (%16lld / iteration)\n"
		   "        %16lld us\n",
		   diff, diff / (RAND_ITER * RAND_REPEAT),
		   diff / CLOCK);
}


int main(int argc, char **argv)
{
	l4_umword_t max_rounds = 1;

	if (argc > 1) {
		max_rounds = strtol(argv[1], 0, 0);
	}

	for (unsigned i = 0; i < max_rounds; ++i) {
		//malloc_test();
		if (0) rm_test();
		access_test();
	}

	//l4util_reboot();

	return 0;
}

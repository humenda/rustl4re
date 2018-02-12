/*
 * superpages/main.cc --
 *
 * 	Test usage of superpages for mapping memory
 *
 * (c) 2013 Björn Döbel <doebel@os.inf.tu-dresden.de>,
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>
#include <errno.h>

#include <l4/util/util.h>
#include <l4/re/env.h>
#include <l4/sys/kdebug.h>
#include <l4/re/env>
#include <l4/re/rm>
#include <l4/re/dataspace>
#include <l4/re/mem_alloc>
#include <l4/re/util/cap_alloc>

/*
 */
static inline long long US(struct timeval& tv)
{
	return tv.tv_sec * 1000000 + tv.tv_usec;
}


static void map_without_superpage()
{
	l4_umword_t size = 1 << 22;
	L4::Cap<L4Re::Dataspace> dscap = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();
	long r = L4Re::Env::env()->mem_alloc()->alloc(size, dscap,
	                                              L4Re::Mem_alloc::Super_pages);
	printf("alloc: %ld\n", r);

	l4_addr_t start = 0;
	r = L4Re::Env::env()->rm()->attach(&start, size, L4Re::Rm::Search_addr,
	                                   dscap, 0, 22);
	printf("attach: %ld addr %lx\n", r, start);

	memset((void*)start, 5, size);
}


static void map_with_superpage()
{
	l4_umword_t size = 1 << 22;
	L4::Cap<L4Re::Dataspace> dscap = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();
	// Hint: note the combined use of Super_pages and Continuous!
	long r = L4Re::Env::env()->mem_alloc()->alloc(size, dscap,
	                                              L4Re::Mem_alloc::Super_pages |
	                                              L4Re::Mem_alloc::Continuous);
	printf("alloc: %ld\n", r);

	l4_addr_t start = 0;
	r = L4Re::Env::env()->rm()->attach(&start, size, L4Re::Rm::Search_addr,
	                                   dscap, 0, 22);
	printf("attach: %ld addr %lx\n", r, start);

	memset((void*)start, 5, size);
}


int main(void)
{
	map_with_superpage();
	map_without_superpage();

	return 0;
}

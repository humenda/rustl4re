/*
 * This file is part of DDE/Linux2.6.
 *
 * (c) 2006-2012 Bjoern Doebel <doebel@os.inf.tu-dresden.de>
 *               Christian Helmuth <ch12@os.inf.tu-dresden.de>
 *     economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

/* Linux */
#include <linux/gfp.h>
#include <linux/string.h>
#include <asm/page.h>

/* DDEKit */
#include <l4/dde/ddekit/memory.h>
#include <l4/dde/ddekit/assert.h>
#include <l4/dde/ddekit/panic.h>

#include "local.h"

int ioprio_best(unsigned short aprio, unsigned short bprio)
{
	WARN_UNIMPL;
	return 0;
}

void *__alloc_bootmem(unsigned long size, unsigned long align,
                      unsigned long goal)
{
	WARN_UNIMPL;
	return 0;
}


/*
 * Stolen from linux-2.6.29/fs/libfs.c
 */
ssize_t memory_read_from_buffer(void *to, size_t count, loff_t *ppos,
                                const void *from, size_t available)
{
	loff_t pos = *ppos;
	if (pos < 0)
		return -EINVAL;
	if (pos > available)
		return 0;
	if (count > available - pos)
		count = available - pos;
	memcpy(to, from + pos, count);
	*ppos = pos + count;

	return count;
}

int capable(int f) { return 1; }

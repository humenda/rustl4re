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

/*
 * \brief    vmalloc implementation
 */

/* Linux */
#include <linux/vmalloc.h>

/* DDEKit */
#include <l4/dde/ddekit/memory.h>
#include <l4/dde/ddekit/lock.h>

void *vmalloc(unsigned long size)
{
	return ddekit_simple_malloc(size);
}

void vfree(const void *addr)
{
	ddekit_simple_free((void*)addr);
}

/******************************************************************************
 * Bjoern Doebel <doebel@tudos.org>                                           *
 *                                                                            *
 * (c) 2005 - 2007 Technische Universitaet Dresden                            *
 * This file is part of DROPS, which is distributed under the terms of the    *
 * GNU General Public License 2. Please see the COPYING file for details.     *
 ******************************************************************************/

#include <linux/kernel.h>	/* printk, ... */
#include <linux/string.h>	/* strcmp, ... */
#include <linux/slab.h>		/* kmalloc, ... */
#include <linux/ctype.h>   /* isspace, toupper */
#include <l4/dde/ddekit/assert.h>	/* assert() */

#define LOG_printf printk

static inline void *malloc(unsigned s)
{
	return kmalloc(s, GFP_KERNEL);
}


static inline void free(void *p)
{
	kfree(p);
}

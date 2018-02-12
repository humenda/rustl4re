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

#include "local.h"

#include <linux/kernel.h>

/* IRQ lock reference counter */
static atomic_t      _refcnt   = ATOMIC_INIT(0);

/* Check whether IRQs are currently disabled.
 *
 * This is the case, if flags is greater than 0.
 */

int raw_irqs_disabled_flags(unsigned long flags)
{
	return ((int)flags > 0);
}

/* Store the current flags state.
 *
 * This is done by returning the current refcnt.
 *
 * XXX: Up to now, flags was always 0 at this point and
 *      I assume that this is always the case. Prove?
 */
unsigned long __raw_local_save_flags(void)
{
	return (unsigned long)atomic_read(&_refcnt);
}

/* Restore IRQ state. */
void raw_local_irq_restore(unsigned long flags)
{
	atomic_set(&_refcnt, flags);
}

/* Disable IRQs by grabbing the IRQ lock. */
void raw_local_irq_disable(void)
{
	atomic_inc(&_refcnt);
}

/* Unlock the IRQ lock until refcnt is 0. */
void raw_local_irq_enable(void)
{
	atomic_set(&_refcnt, 0);
}


void raw_safe_halt(void)
{
	WARN_UNIMPL;
}


void halt(void)
{
	WARN_UNIMPL;
}

/* These functions are empty for DDE. Every DDE thread is a separate
 * "virtual" CPU. Therefore there is no need to en/disable bottom halves.
 */
void local_bh_disable(void) {}
void __local_bh_enable(void) {}
void _local_bh_enable(void) {}
void local_bh_enable(void) {}

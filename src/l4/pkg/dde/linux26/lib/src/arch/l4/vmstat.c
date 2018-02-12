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

#include <linux/fs.h>

atomic_long_t vm_stat[NR_VM_ZONE_STAT_ITEMS];


void dec_zone_page_state(struct page *page, enum zone_stat_item item)
{
	WARN_UNIMPL;
}


void inc_zone_page_state(struct page *page, enum zone_stat_item item)
{
	WARN_UNIMPL;
}


void __inc_zone_page_state(struct page *page, enum zone_stat_item item)
{
	WARN_UNIMPL;
}

void __get_zone_counts(unsigned long *active, unsigned long *inactive,
                       unsigned long *free, struct pglist_data *pgdat)
{
	WARN_UNIMPL;
}

void __dec_zone_state(struct zone *zone, enum zone_stat_item item)
{
	WARN_UNIMPL;
}

void __inc_zone_state(struct zone *zone, enum zone_stat_item item)
{
	WARN_UNIMPL;
}

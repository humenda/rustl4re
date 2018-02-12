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

/* Dummy functions for power management. */

#include "local.h"
#include <linux/device.h>

int device_pm_add(struct device * dev)
{
	WARN_UNIMPL;
	return 0;
}


void device_pm_remove(struct device * dev)
{
	WARN_UNIMPL;
}

int pm_qos_add_requirement(int qos, char *name, s32 value) { return 0; }
int pm_qos_update_requirement(int qos, char *name, s32 new_value) { return 0; }
void pm_qos_remove_requirement(int qos, char *name) { }
int pm_qos_requirement(int qos) { return 0; }
int pm_qos_add_notifier(int qos, struct notifier_block *notifier) {  return 0; }
int pm_qos_remove_notifier(int qos, struct notifier_block *notifier) { return 0; }

/*
 * (c) 2013 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include <l4/sys/compiler.h>
#include <l4/vbus/vbus_types.h>
#include <l4/sys/types.h>

__BEGIN_DECLS

int L4_CV
l4vbus_pm_suspend(l4_cap_idx_t vbus, l4vbus_device_handle_t handle);

int L4_CV
l4vbus_pm_resume(l4_cap_idx_t vbus, l4vbus_device_handle_t handle);

__END_DECLS

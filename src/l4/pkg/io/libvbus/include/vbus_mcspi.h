/*
 * (c) 2009 Torsten Frenzel <frenzel@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include <l4/sys/compiler.h>
#include <l4/sys/types.h>
#include <l4/vbus/vbus_types.h>

__BEGIN_DECLS

int L4_CV
l4vbus_mcspi_read(l4_cap_idx_t vbus, l4vbus_device_handle_t handle,
                  unsigned channel, l4_umword_t *value);

int L4_CV
l4vbus_mcspi_write(l4_cap_idx_t vbus, l4vbus_device_handle_t handle,
                   unsigned channel, l4_umword_t value);

__END_DECLS

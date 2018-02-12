/*
 * (c) 2010 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *          Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include <sys/cdefs.h>
#include <l4/sys/compiler.h>

__BEGIN_DECLS

L4_EXPORT
void libpciids_name_device(char *name, int len,
                           unsigned vendor, unsigned device);

__END_DECLS

/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>,
 *               Frank Mehnert <fm3@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#ifndef MODULE_H
#define MODULE_H

#include <stddef.h>
#include <l4/util/mb_info.h>
#include <l4/sys/compiler.h>

EXTERN_C_BEGIN

void print_module_name(const char *name, const char *alt_name);

EXTERN_C_END

#endif

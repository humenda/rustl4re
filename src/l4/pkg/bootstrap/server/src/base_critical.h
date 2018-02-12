/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Frank Mehnert <fm3@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#ifndef BASE_CRITICAL_H
#define BASE_CRITICAL_H

#include <l4/sys/compiler.h>

EXTERN_C_BEGIN

void base_critical_enter(void);
void base_critical_leave(void);

EXTERN_C_END

#endif

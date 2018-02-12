/*
 * This file is part of DDEKit.
 *
 * (c) 2006-2012 Bjoern Doebel <doebel@os.inf.tu-dresden.de>
 *               Christian Helmuth <ch12@os.inf.tu-dresden.de>
 *               Thomas Friebel <tf13@os.inf.tu-dresden.de>
 *     economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#pragma once

#include <l4/sys/compiler.h>

EXTERN_C_BEGIN

/** \defgroup DDEKit_util */

/** Panic - print error message and enter the kernel debugger.
 * \ingroup DDEKit_util
 */
void ddekit_panic(char const *fmt, ...) __attribute__((noreturn));

/** Print a debug message.
 * \ingroup DDEKit_util
 */
void ddekit_debug(char const *fmt, ...);

/** Print current backtrace.
 *
 * \ingroup DDEKit_util
 */
void ddekit_backtrace(void);

EXTERN_C_END

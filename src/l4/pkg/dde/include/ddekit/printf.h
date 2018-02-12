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
#include <stdarg.h>

EXTERN_C_BEGIN

/** Print message.
 * \ingroup DDEKit_util
 */
void ddekit_print(const char *);

/** Print message with format.
 * \ingroup DDEKit_util
 */
void ddekit_printf(const char *fmt, ...);

/** Print message with format list.
 * \ingroup DDEKit_util
 */
void ddekit_vprintf(const char *fmt, va_list va);

/** Log function and message.
 * \ingroup DDEKit_util
 */
#define ddekit_log(doit, msg...) \
	do {                                       \
		if (doit) {                            \
			ddekit_printf("%s(): ", __func__); \
			ddekit_printf(msg);                \
			ddekit_printf("\n");               \
		}                                      \
	} while(0);

EXTERN_C_END

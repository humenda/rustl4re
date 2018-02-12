/*
 * \brief   printf and helpers
 * \author  Thomas Friebel <tf13@os.inf.tu-dresden.de>
 * \author  Christian Helmuth <ch12@os.inf.tu-dresden.de>
 * \author  Bjoern Doebel<doebel@os.inf.tu-dresden.de>
 * \date    2008-08-26
 */

/*
 * (c) 2006-2008 Technische Universität Dresden
 * This file is part of TUD:OS, which is distributed under the terms of the
 * GNU General Public License 2. Please see the COPYING file for details.
 */

#pragma once

#include <l4/sys/compiler.h>

#include <stdarg.h>

namespace DDEKit
{

/** Print message.
 * \ingroup DDEKit_util
 */
void print(const char *);

/** Print message with format.
 * \ingroup DDEKit_util
 */
void printf(const char *fmt, ...);

/** Print message with format list.
 * \ingroup DDEKit_util
 */
void vprintf(const char *fmt, va_list va);

/** Log function and message.
 * \ingroup DDEKit_util
 */
void log(bool const doit, const char *fmt, ...);

}

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

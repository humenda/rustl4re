/*
 * \brief   Logging facility with printf()-like interface
 */

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

#include <l4/dde/ddekit/printf.h>

#include <stdio.h>

/**
 * Log constant string message w/o arguments
 *
 * \param msg  message to be logged
 */
void ddekit_print(const char *msg)
{
	printf("%s", msg);
}

/**
 * Log message with print()-like arguments
 *
 * \param fmt  format string followed by optional arguments
 */
void ddekit_printf(const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	ddekit_vprintf(fmt, va);
	va_end(va);
}

/* Log message with vprintf()-like arguments
 *
 * \param fmt  format string
 * \param va   variable argument list
 */
void ddekit_vprintf(const char *fmt, va_list va)
{
	vprintf(fmt, va);
}

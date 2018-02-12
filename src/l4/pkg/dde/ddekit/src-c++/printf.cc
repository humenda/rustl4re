/*
 * \brief   Logging facility with printf()-like interface
 * \author  Thomas Friebel <yaron@yaron.de>
 * \author  Bjoern Doebel<doebel@os.inf.tu-dresden.de>
 * \date    2006-03-01
 */

/*
 * (c) 2006-2008 Technische Universität Dresden
 * This file is part of TUD:OS, which is distributed under the terms of the
 * GNU General Public License 2. Please see the COPYING file for details.
 */

#include <l4/dde/ddekit/printf.h>
#include <stdio.h>

/**
 * Log constant string message w/o arguments
 *
 * \param msg  message to be logged
 */
void DDEKit::print(const char *msg)
{
	DDEKit::printf("%s", msg);
}

/**
 * Log message with print()-like arguments
 *
 * \param fmt  format string followed by optional arguments
 */
void DDEKit::printf(const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	DDEKit::vprintf(fmt, va);
	va_end(va);
}

/* Log message with vprintf()-like arguments
 *
 * \param fmt  format string
 * \param va   variable argument list
 */
void DDEKit::vprintf(const char *fmt, va_list va)
{
	::vprintf(fmt, va);
}

void DDEKit::log(bool const doit, const char *fmt, ...)
{
	if (doit)
	{
		va_list va;

		va_start(va, fmt);
		DDEKit::printf("%s(): ", __func__);
		DDEKit::vprintf(fmt, va);
		va_end(va);
		DDEKit::printf("\n");
	}
}

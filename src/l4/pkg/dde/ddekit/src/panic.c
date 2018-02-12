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

#include <l4/dde/ddekit/panic.h>
#include <l4/dde/ddekit/printf.h>

#include <l4/sys/kdebug.h>
#include <l4/util/backtrace.h>
#include <stdarg.h>

void ddekit_panic(const char *fmt, ...) {
	va_list va;

	va_start(va, fmt);
	ddekit_vprintf(fmt, va);
	va_end(va);
	ddekit_printf("\n");

	ddekit_backtrace();

	while (1)
		enter_kdebug("ddekit_panic()");
}

void ddekit_debug(const char *fmt, ...) {
	va_list va;

	va_start(va, fmt);
	ddekit_vprintf(fmt, va);
	va_end(va);
	ddekit_printf("\n");

	ddekit_backtrace();
	enter_kdebug("ddekit_debug()");
}

void ddekit_backtrace()
{
	int len = 16;
	void *array[len];
	unsigned i, ret = l4util_backtrace(&array[0], len);

	ddekit_printf("backtrace:\n");
	for (i = 0; i < ret; ++i)
		ddekit_printf("\t%p\n", array[i]);
}

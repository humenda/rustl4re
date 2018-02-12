#pragma once

#include <l4/dde/ddekit/printf.h>
#include <l4/dde/ddekit/panic.h>

/** \file ddekit/assert.h */

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

/** Assert that an expression is true and panic if not. 
 * \ingroup DDEKit_util
 */
#define Assert(expr)	do 									\
	{														\
		if (!(expr)) {										\
			ddekit_print("\033[31;1mDDE: Assertion failed: "#expr"\033[0m\n");	\
			ddekit_printf("  File: %s:%d\n",__FILE__,__LINE__); 		\
			ddekit_printf("  Function: %s()\n", __FUNCTION__);	\
			ddekit_panic("Assertion failed.");				\
		}} while (0);

#define assert Assert

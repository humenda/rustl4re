/*
 * \brief   Assertions
 * \author  Christian Helmuth
 * \author  Bjoern Doebel
 * \date    2008-08-26
 */

/*
 * (c) 2006-2008 Technische Universität Dresden
 * This file is part of TUD:OS, which is distributed under the terms of the
 * GNU General Public License 2. Please see the COPYING file for details.
 */

#pragma once

#include <l4/sys/compiler.h>

#include <l4/dde/ddekit/printf.h>
#include <l4/dde/ddekit/panic.h>


/** \file ddekit/assert.h */

/** Assert that an expression is true and panic if not. 
 * \ingroup DDEKit_util
 */
#define Assert(expr)	do 									\
	{														\
		if (!(expr)) {										\
			DDEKit::print("\033[31;1mDDE: Assertion failed: "#expr"\033[0m\n");	\
			DDEKit::printf("  File: %s:%d\n",__FILE__,__LINE__); 		\
			DDEKit::printf("  Function: %s()\n", __FUNCTION__);	\
			DDEKit::panic("Assertion failed.");				\
		}} while (0);

#define assert Assert

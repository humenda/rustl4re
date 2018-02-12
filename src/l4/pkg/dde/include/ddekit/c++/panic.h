/*
 * \brief   Panic() and Debug printf()
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

namespace DDEKit
{

/** \defgroup DDEKit_util */

/** Panic - print error message and enter the kernel debugger.
 * \ingroup DDEKit_util
 */
void panic(const char *fmt, ...) __attribute__((noreturn));

/** Print a debug message.
 * \ingroup DDEKit_util
 */
void debug(const char *fmt, ...);

}

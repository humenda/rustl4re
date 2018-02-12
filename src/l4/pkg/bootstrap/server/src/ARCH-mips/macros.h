/*
 * This file is distributed under the terms of the GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 *
 * Copyright (C) 2014 Imagination Technologies Ltd.
 * Author: Yann Le Du <ledu@kymasys.com>
 */

#ifndef __ARCH_MIPS_MACROS_H__
#define __ARCH_MIPS_MACROS_H__

#ifdef __mips64
#include "ARCH-amd64/macros.h"
#else
#include "ARCH-x86/macros.h"
#endif

#endif  /* ! __ARCH_MIPS_MACROS_H__ */

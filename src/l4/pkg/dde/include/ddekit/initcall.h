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

// from l4/sys/compiler.h
#if (__GNUC__ == 3 && __GNUC_MINOR__ >= 3) || __GNUC__ >= 4
#define L4_STICKY(x)    __attribute__((used)) x
#else
#define L4_STICKY(x)    __attribute__((unused)) x
#endif

#define l4str(s) #s

// from dde_linux/ARCH-x86/ctor.h
typedef void (*l4ddekit_initcall_t)(void);

/** Define a function to be a DDEKit initcall. 
 *
 *  Define a function to be a DDEKit initcall. This function will then be placed
 *  in a separate linker section of the binary (called .l4dde_ctors). The L4Env
 *  construction mechanism will execute all constructors in this section during
 *  application startup.
 *
 *  This is the right place to place Linux' module_init functions & Co.
 *
 *  \param fn    function
 */
#define DDEKIT_INITCALL(fn)       DDEKIT_CTOR(fn, 65535)


#define DDEKIT_CTOR(func, prio) \
  L4_DECLARE_CONSTRUCTOR(func, prio)

/**
 * Runs all registered initcalls.
 */
EXTERN_C void ddekit_do_initcalls(void);


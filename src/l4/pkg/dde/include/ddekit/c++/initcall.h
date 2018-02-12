/*
 * \brief   Init calls
 * \author  Thomas Friebel <tf13@os.inf.tu-dresden.de>
 * \author  Christian Helmuth <ch12@os.inf.tu-dresden.de>
 * \author  Bjoern Doebel <doebel@os.inf.tu-dresden.de>
 * \date    2007-01-26
 */

/*
 * (c) 2007-2008 Technische Universität Dresden
 * This file is part of TUD:OS, which is distributed under the terms of the
 * GNU General Public License 2. Please see the COPYING file for details.
 */

#pragma once

typedef void (*ctor_fn)(void);

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
#define DDEKIT_CTOR(fn, prio) \
	ctor_fn const __l4ctor_##fn __attribute((used)) \
	__attribute__((__section__(".dde_ctors." #prio))) = fn

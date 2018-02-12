/*****************************************************************************/
/**
 * \file   slab/lib/include/__debug.h
 * \brief  Debug config.
 *
 * \date   2006-12-18
 * \author Lars Reuther <reuther@os.inf.tu-dresden.de>
 * \author Christian Helmuth <ch12@os.inf.tu-dresden.de>
 */
/*****************************************************************************/

/*
 * (c) 2006-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#ifndef _SLAB___DEBUG_H
#define _SLAB___DEBUG_H

/* 1 enables debug output, 0 disables */
#define DEBUG_SLAB_INIT         0
#define DEBUG_SLAB_GROW         0
#define DEBUG_SLAB_ASSERT       1

#if !DEBUG_SLAB_ASSERT
#undef Assert
#define Assert(expr) do { } while (0)
#endif

#endif /* !_L4ENV___DEBUG_H */

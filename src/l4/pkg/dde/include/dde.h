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

#ifndef l4_ddekit_h
#define l4_ddekit_h

/* FIXME if this is ddekit.h, it should be moved into dde/ddekit/include/ddekit.h (also
 * all headers under include/ddekit) */

/**
 * Initialize the DDE. Must be called before any other DDE function.
 *
 * FIXME revisit this one
 */
void ddekit_init(void);
void ddekit_do_initcalls(void);

#endif

/**
 * \file	l4con/include/l4con_ev.h
 * \brief	console protocol definitions - input event part
 *
 * \date	2001
 * \author	Christian Helmuth <ch12@os.inf.tu-dresden.de>
 *
 * These macros are used as parameters for the IDL functions. */
/*
 * (c) 2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#ifndef _L4CON_L4CON_EV_H
#define _L4CON_L4CON_EV_H

#include <l4/sys/linkage.h>
#include <l4/sys/types.h>

/* we use the original 'libinput' event definitions */
/* except: */

/** Event type */
#define EV_CON		0x10

/** EV_CON event codes */
#define EV_CON_RESERVED    0		/**< invalid request */
#define EV_CON_REDRAW      1		/**< requests client redraw */
#define EV_CON_BACKGROUND  2		/**< tells client that it looses
					     the framebuffer */

__BEGIN_DECLS

L4_CV int l4con_map_keyinput_to_ascii(unsigned value, unsigned shift);

__END_DECLS

#endif

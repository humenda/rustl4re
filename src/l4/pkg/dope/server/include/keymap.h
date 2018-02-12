/*
 * \brief   Interface of keymap module
 * \date    2002-11-13
 * \author  Norman Feske <nf2@inf.tu-dresden.de>
 */

/*
 * Copyright (C) 2002-2004  Norman Feske  <nf2@os.inf.tu-dresden.de>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the DOpE package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */

#ifndef _DOPE_KEYMAP_H_
#define _DOPE_KEYMAP_H_


#define KEYMAP_SWITCH_LSHIFT    0x01
#define KEYMAP_SWITCH_RSHIFT    0x02
#define KEYMAP_SWITCH_LCONTROL  0x04
#define KEYMAP_SWITCH_RCONTROL  0x08
#define KEYMAP_SWITCH_ALT       0x10
#define KEYMAP_SWITCH_ALTGR     0x11


struct keymap_services {
	char    (*get_ascii) (long keycode,long switches);
};

#endif /* _DOPE_KEYMAP_H_ */


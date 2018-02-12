/*!
 * \file	x86emu/include/int10.h
 * \brief	console protocol definitions.
 *
 * \date	2005
 * \author	Frank Mehnert <fm3@os.inf.tu-dresden.de>
 *
 * These macros are used as parameters for the IDL functions. */
/*
 * (c) 2005-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */


#ifndef _X86EMU_INT10_H
#define _X86EMU_INT10_H

#include <l4/sys/linkage.h>
#include <l4/util/mb_info.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

/**
 * Set a VESA video mode.
 *
 * \param  mode       video mode, use ~0 to automatically choose 'best' one
 * \retval ctrl_info  VESA controller info
 * \retval mode_info  VESA mode info
 * \return         0  Success
 *              != 0  Failure
 *
 * See VESA Specification 3.0.
 */
L4_CV int x86emu_int10_set_vbemode(int mode, l4util_mb_vbe_ctrl_t *ctrl_info,
                                   l4util_mb_vbe_mode_t *mode_info);

/**
 * Pan the graphics memory.
 *
 * \param x           horizontal offset into graphics memory
 * \param y           horizontal offset into graphics memory
 * \return         0  Success
 *              != 0  Failure
 */
L4_CV int x86emu_int10_pan(unsigned *x, unsigned *y);

/**
 * Release all memory occupied by the int10 emulator.
 *
 * \return         0  Success
 *              != 0  Failure
 */
L4_CV int x86emu_int10_done(void);

__END_DECLS

#endif

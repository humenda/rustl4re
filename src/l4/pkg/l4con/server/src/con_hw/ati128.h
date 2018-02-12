/*!
 * \file	ati.h
 * \brief	ATI Rage128 driver
 *
 * \date	08/2003
 * \author	Frank Mehnert <fm3@os.inf.tu-dresden.de> */
/*
 * (c) 2003-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#ifndef __ATI128_H_
#define __ATI128_H_

void ati128_register(void);
void ati128_vid_init(con_accel_t *accel);
void ati128_test_for_card_disappeared(void);

#endif


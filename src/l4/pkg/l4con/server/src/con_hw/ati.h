/*!
 * \file	ati.h
 * \brief	ATI Mach64 driver
 *
 * \date	07/2002
 * \author	Frank Mehnert <fm3@os.inf.tu-dresden.de> */
/*
 * (c) 2002-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#ifndef __ATI_H_
#define __ATI_H_

void ati_register(void);
void ati_vid_init(con_accel_t *accel);
void ati_test_for_card_disappeared(void);

#endif


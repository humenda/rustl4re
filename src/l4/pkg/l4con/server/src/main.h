/**
 * \file	con/server/src/main.h
 * \brief	some global structures
 *
 * \date	2004
 * \author	Frank Mehnert <fm3@os.inf.tu-dresden.de> */
/*
 * (c) 2003-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#ifndef MAIN_H
#define MAIN_H

#include <l4/sys/types.h>
#include <pthread.h>

extern struct l4con_vc *vc[];
extern pthread_mutex_t want_vc_lock;
extern l4_uint8_t  vc_mode;
extern l4_cap_idx_t ev_partner_l4id, vc_partner_l4id;
extern int want_vc, fg_vc;
extern int noaccel, pan, use_s0, vbemode, use_fastmemcpy, cpu_load_history;
extern int update_id;

void request_vc(int nr);
void request_vc_delta(int delta);

#endif

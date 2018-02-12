/**
 * \file	con/server/src/vc.h
 * \brief	internals of `con' submodule, thread specific vc stuff
 *
 * \date	2001
 * \author	Christian Helmuth <ch12@os.inf.tu-dresden.de>
 * 		Frank Mehnert <fm3@os.inf.tu-dresden.de> */
/*
 * (c) 2008-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

/* (c) 2003 'Technische Universitaet Dresden'
 * This file is part of the con package, which is distributed under
 * the terms of the GNU General Public License 2. Please see the
 * COPYING file for details. */

#ifndef _VC_H
#define _VC_H

#include <l4/re/c/util/video/goos_fb.h>

#include "l4con.h"
#include "con_hw/init.h"

void vc_init(void);
void vc_map_env_fb(void);
void vc_get_env_fb(void);
void vc_loop(struct l4con_vc *this_vc);
void vc_show_id(struct l4con_vc *this_vc);
void vc_show_dmphys_poolsize(struct l4con_vc *this_vc);
void vc_show_cpu_load(struct l4con_vc *this_vc);
void vc_show_timer_ticks(struct l4con_vc *this_vc);
void vc_show_drops_cscs_logo(void);
void vc_clear(struct l4con_vc *this_vc);
void vc_show_fresh_console_logo(struct l4con_vc *this_vc);
int  vc_close(struct l4con_vc *this_vc);

void vc_brightness_contrast(int diff_brightness, int diff_contrast);

extern pslim_copy_fn fg_do_copy;
extern pslim_copy_fn bg_do_copy;
extern pslim_fill_fn fg_do_fill;
extern pslim_fill_fn bg_do_fill;
extern pslim_sync_fn fg_do_sync;
extern pslim_sync_fn bg_do_sync;
extern pslim_drty_fn fg_do_drty;

extern con_accel_t hw_accel;
extern l4_umword_t status_area;

extern l4re_util_video_goos_fb_t goosfb;
extern l4re_video_view_info_t fb_info;

EXTERN_C_BEGIN
struct l4con_vc *alloc_vc(void);
void register_fb_ds(struct l4con_vc *vc);
EXTERN_C_END

#endif /* !_VC_H */

/**
 * \file
 * \brief	header file
 *
 * \date	Oct 2008
 * \author	Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 */
/*
 * (c) 2008-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#pragma once

EXTERN_C_BEGIN

#include <l4/input/libinput.h>

int server_loop(void);

long con_if_open_component(short vfbmode);
long con_vc_direct_update_component(struct l4con_vc *vc,
                                    const l4con_pslim_rect_t *rect);
long con_vc_close_component(struct l4con_vc *vc);
long
con_vc_pslim_fill_component(struct l4con_vc *vc,
			    const l4con_pslim_rect_t *rect,
			    l4con_pslim_color_t color);

long
con_vc_pslim_copy_component(struct l4con_vc *vc,
			    const l4con_pslim_rect_t *rect,
			    l4_int16_t dx, l4_int16_t dy);

long
con_vc_puts_component(struct l4con_vc *vc,
                      const char *s,
                      int len,
                      short x,
                      short y,
                      l4con_pslim_color_t fg_color,
                      l4con_pslim_color_t bg_color);
long
con_vc_puts_scale_component(struct l4con_vc *vc,
                            const char *s,
                            int len,
                            short x,
                            short y,
                            l4con_pslim_color_t fg_color,
                            l4con_pslim_color_t bg_color,
                            short scale_x,
                            short scale_y);

void fill_out_info(struct l4con_vc *vc);
void create_event(struct l4con_vc *vc);

void send_event_client(struct l4con_vc *vc, struct l4input *ev);

void periodic_work(void);

EXTERN_C_END

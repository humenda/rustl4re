/**
 * \file	l4con/server/src/ev.c
 * \brief	mouse, keyboard, etc event stuff
 *
 * \date	2001
 * \author	Christian Helmuth <ch12@os.inf.tu-dresden.de>
 *		Frank Mehnert <fm3@os.inf.tu-dresden.de> */
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

#include <l4/l4con/l4con.h>
#include <l4/util/l4_macros.h>
#include <l4/input/libinput.h>
#include <l4/sys/kdebug.h>
#include <l4/sys/factory.h>
#include <l4/sys/icu.h>
#include <l4/re/env.h>
#include <l4/re/c/event_buffer.h>
#include <l4/re/c/util/cap_alloc.h>
#include <l4/re/c/util/video/goos_fb.h>

#include <pthread-l4.h>
#include <stdlib.h>
#include <stdio.h>

#include "config.h"
#include "ev.h"
#include "main.h"
#include "vc.h"
#include "srv.h"

int nomouse, noshift, gen_abs_events;
int cur_abs_pos_x, cur_abs_pos_y;
static l4_cap_idx_t ev_ds, ev_irq;
static l4re_event_buffer_consumer_t ev_buf;

/** brief Key event handling -> distribution and switch */
static void handle_event(struct l4input *ev)
{
  static int altgr_down;
  static int shift_down;
  static struct l4input special_ev = { .type = 0xff };

  if (ev->type == EV_KEY)
    {
      l4_umword_t keycode      = ev->code;
      l4_umword_t down         = ev->value;
      l4_umword_t special_down = 0;

      if (nomouse && keycode >= BTN_MOUSE && keycode < BTN_TASK)
	return;

      if (keycode == KEY_RIGHTALT)
	altgr_down = special_down = down;
      else if (!noshift
               && (keycode == KEY_LEFTSHIFT || keycode == KEY_RIGHTSHIFT))
        shift_down = special_down = down;

      if (special_down)
	{
	  /* Defer sending of the special key until we know if we handle
	   * the next key completely in the server */
	  special_ev = *ev;
	  return;
	}

      if (down && (altgr_down || shift_down))
	{
	  /* virtual console switching */
	  if (keycode >= KEY_F1 && keycode <= KEY_F10)
	    {
	      request_vc(keycode - KEY_F1 + 1);
	      special_ev.type = 0xff;
	      return;
	    }
	  if (keycode == KEY_LEFT)
	    {
	      request_vc_delta(-1);
	      special_ev.type = 0xff;
	      return;
	    }
	  if (keycode == KEY_RIGHT)
	    {
	      request_vc_delta(1);
	      special_ev.type = 0xff;
	      return;
	    }
	  if (keycode == KEY_F11 && altgr_down)
	    {
	      /* F11/Shift F11: increase/decrase brightness */
	      vc_brightness_contrast(shift_down ? -100 : 100, 0);
	      special_ev.type = 0xff;
	      return;
	    }
	  if (keycode == KEY_F12 && altgr_down)
	    {
	      /* F12/Shift F12: increase/decrase contrast */
	      vc_brightness_contrast(0, shift_down ? -100 : 100);
	      special_ev.type = 0xff;
	      return;
	    }
	  if (keycode == KEY_PAUSE && altgr_down)
	    {
	      cpu_load_history = 1-cpu_load_history;
	      return;
	    }
#ifndef L4BID_RELEASE_MODE
	  if (keycode == KEY_SYSRQ && altgr_down)
	    {
	      /* Magic SysReq -> enter_kdebug() */
	      enter_kdebug("AltGr + SysRq");
	      special_ev.type = 0xff;
	      return;
	    }
#endif
	}

      /* No special key, send deferred key event */
      if (special_ev.type != 0xff)
	{
	  send_event_client(vc[fg_vc], &special_ev);
	  special_ev.type = 0xff;
	}
    }
  else if (ev->type == EV_REL || ev->type == EV_ABS)
    {
      /* mouse event */
      if (nomouse)
	return;

      if (gen_abs_events && ev->type == EV_REL)
        {
          // for now we treat gen_abs_events to replace the relative events
          ev->type = EV_ABS;
          if (ev->code == 0)
            {
              cur_abs_pos_x += ev->value;
              if (cur_abs_pos_x < 0)
                cur_abs_pos_x = 0;
              else if ((unsigned)cur_abs_pos_x >= vc[fg_vc]->client_xres)
                cur_abs_pos_x = vc[fg_vc]->client_xres;
              ev->value = cur_abs_pos_x;
            }
          else if (ev->code == 1)
            {
              cur_abs_pos_y += ev->value;
              if (cur_abs_pos_y < 0)
                cur_abs_pos_y = 0;
              else if ((unsigned)cur_abs_pos_y >= vc[fg_vc]->client_yres)
                cur_abs_pos_y = vc[fg_vc]->client_yres;
              ev->value = cur_abs_pos_y;
            }
        }
    }
  else if (ev->type == EV_MSC)
    {
      /* ignored */
      return;
    }
  else if (ev->type == EV_SYN)
    {
      /* Pass through */
    }
  else
    {
      printf("handle_event: Unknown event type %d\n", ev->type);
      return;
    }

  send_event_client(vc[fg_vc], ev);
}

L4_CV static void handle_input_l4i(struct l4input *ev)
{
  handle_event(ev);
}

L4_CV static void handle_event_ev(l4re_event_t *ev, void *data)
{
  (void)data;
  struct l4input *_ev = (struct l4input *)ev;
  handle_event(_ev);
}

static void *ev_thread(void *d)
{
  (void)d;
  l4re_event_buffer_consumer_process(&ev_buf, ev_irq,
                                     pthread_l4_cap(pthread_self()), NULL,
                                     handle_event_ev);
  return NULL;
}

#define Panic(a...) do { fprintf(stderr, a); exit(1); } while (0)

/** \brief event driver initialization
 */
void
ev_init()
{
  pthread_t t;

  if (l4_is_invalid_cap(ev_ds = l4re_util_cap_alloc()))
    Panic("Cap alloc failed");

  if (l4_is_invalid_cap(ev_irq = l4re_util_cap_alloc()))
    Panic("Cap alloc failed");

  if (l4_error(l4_factory_create_irq(l4re_env()->factory, ev_irq)))
    Panic("IRQ create failed");

  if (l4_error(l4_icu_bind(l4re_util_video_goos_fb_goos(&goosfb), 0, ev_irq))
      || l4re_event_get_buffer(l4re_util_video_goos_fb_goos(&goosfb), ev_ds)
      || l4re_event_buffer_attach(&ev_buf, ev_ds, l4re_env()->rm))
    {
      // failed, try to start the hw driver
      l4re_util_cap_free_um(ev_ds);
      l4re_util_cap_free_um(ev_irq);

      printf("Using l4input\n");
      l4input_init(254, handle_input_l4i);
      return;
    }

  printf("Using l4re-console event pass-through\n");

  if (pthread_create(&t, NULL, ev_thread, NULL))
    Panic("Thread creation failed\n");
}

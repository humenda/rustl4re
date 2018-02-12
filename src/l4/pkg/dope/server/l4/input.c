/*
 * \brief   DOpE input module
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

/*** L4 INCLUDES ***/
#include <l4/input/libinput.h>
#include <l4/sys/factory.h>
#include <l4/sys/icu.h>
#include <l4/re/c/event_buffer.h>
#include <l4/re/c/video/view.h>
#include <l4/re/env.h>
#include <l4/re/c/util/cap_alloc.h>
#include <l4/re/c/util/video/goos_fb.h>

/*** LOCAL INCLUDES ***/
#include "dopestd.h"
#include "event.h"
#include "input.h"

#define Panic(a...) do { fprintf(stderr, a); exit(1); } while (0)

static l4_cap_idx_t ev_ds, ev_irq;
static l4re_event_buffer_consumer_t ev_buf;

int init_input(struct dope_services *d);

/*************************
 *** SERVICE FUNCTIONS ***
 *************************/

static void convert_event(struct l4input *from, EVENT *to)
{
  to->time = from->time;
  to->type = from->type;
  to->code = from->code;
  to->value = from->value;
  to->stream_id = from->stream_id;
}

/*** GET NEXT EVENT OF EVENT QUEUE ***
 *
 * \return  0 if there is no pending event
 *          1 if there an event was returned in out parameter e
 */
#define MAX_INPUT_EVENTS 64
static struct l4input ev[MAX_INPUT_EVENTS];
static long num_ev, curr_ev;

static int get_event_null(EVENT *e)
{
  (void)e;
  return 0;
}

static int get_event_libinput(EVENT *e)
{
  /* poll some new input events from libinput */
  if (curr_ev == num_ev) {
      curr_ev = 0;
      num_ev  = l4input_flush(ev, MAX_INPUT_EVENTS);
  }

  /* no event in queue -> return */
  if (curr_ev == num_ev)
    return 0;

  convert_event(&ev[curr_ev], e);
  curr_ev++;
  return 1;
}

static int get_event_l4re_event(EVENT *e)
{
  l4re_event_t *v = l4re_event_buffer_next(&ev_buf);

  if (v == NULL)
    return 0;

  convert_event((struct l4input *)v, e);
  l4re_event_free(v);
  return 1;
}

/****************************************
 *** SERVICE STRUCTURE OF THIS MODULE ***
 ****************************************/

static struct input_services input;

/**************************
 *** MODULE ENTRY POINT ***
 **************************/

int init_input(struct dope_services *d)
{
  extern l4re_util_video_goos_fb_t goosfb;
  extern int goosfb_initialized;
  extern void *fb_buf;
  int init_ret;

  input.get_event = get_event_null;

  d->register_module("Input 1.0", &input);

  if (!goosfb_initialized
      && l4re_util_video_goos_fb_setup_name(&goosfb, "fb"))
    {
      printf("video init failed\n");
      return 0;
    }
  goosfb_initialized = 1;

  fb_buf = l4re_util_video_goos_fb_attach_buffer(&goosfb);
  if (!fb_buf)
    {
      printf("video memory init failed\n");
      return 0;
    }

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
      // failed, try to get 'ev'
      l4_cap_idx_t evcap;
      if (l4_is_invalid_cap(evcap = l4re_util_cap_alloc()))
        Panic("Cap alloc failed");

      evcap = l4re_env_get_cap("ev");

      if (l4_is_invalid_cap(evcap)
          || l4_error(l4_icu_bind(evcap, 0, ev_irq))
          || l4re_event_get_buffer(evcap, ev_ds)
          || l4re_event_buffer_attach(&ev_buf, ev_ds, l4re_env()->rm))
        {
          // failed, try to start the hw driver
          l4re_util_cap_free(ev_ds);
          l4re_util_cap_free(ev_irq);

          printf("Using native hardware drivers\n");

          init_ret = l4input_init(254, NULL);
          if (init_ret)
            printf("Input(init): Error: l4input_init() returned %d\n", init_ret);
          else
            input.get_event = get_event_libinput;

          return 1;
        }

      printf("Using L4Re::Event interface via 'ev'.\n");

      input.get_event = get_event_l4re_event;

      return 1;
    }

  printf("Using L4Re::Event via Framebuffer\n");

  input.get_event = get_event_l4re_event;

  return 1;
}

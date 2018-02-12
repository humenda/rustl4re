/*
 * \brief   DOpE client library - L4 specific initialisation
 * \date    2002-11-13
 * \author  Norman Feske <nf2@inf.tu-dresden.de>
 */

/*
 * Copyright (C) 2002-2003  Norman Feske  <nf2@os.inf.tu-dresden.de>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the DOpE package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */

/*** GENERAL INCLUDES ***/
#include <stdio.h>
#include <stdlib.h>

/*** L4 INCLUDES ***/
#include <l4/util/util.h>
#include <l4/sys/factory.h>
#include <l4/sys/icu.h>
#include <l4/re/c/namespace.h>
#include <l4/re/c/util/cap_alloc.h>
#include <l4/re/c/dataspace.h>
#include <l4/re/c/event.h>
#include <l4/re/c/event_buffer.h>
#include <l4/re/c/rm.h>

#include <pthread-l4.h>

/*** LOCAL INCLUDES ***/
#include "dopestd.h"
#include "dopelib.h"
#include "dopedef.h"
#include "sync.h"

l4_cap_idx_t dope_server;
l4re_ds_t ev_ds;
l4_cap_idx_t ev_irq;
static l4re_event_buffer_consumer_t ev_buf;

struct dopelib_mutex *dopelib_cmdf_mutex;
struct dopelib_mutex *dopelib_cmd_mutex;


void dopelib_usleep(int usec);
void dopelib_usleep(int usec) {
	l4_sleep(usec/1000);
}

L4_CV void dope_process_event(l4re_event_t *ev, void *data);

/*** INTERFACE: HANDLE EVENTLOOP OF A DOpE CLIENT ***/
L4_CV void dope_eventloop(void)
{
  l4re_event_buffer_consumer_process(&ev_buf, ev_irq,
                                     pthread_l4_cap(pthread_self()), NULL,
                                     dope_process_event);
}

L4_CV l4re_event_t *dope_event_get(void)
{
  return l4re_event_buffer_next(&ev_buf);
}

/*** INTERFACE: CONNECT TO DOpE SERVER AND INIT CLIENT LIB ***
 *
 * return 0 on success, != 0 otherwise
 */
L4_CV long dope_init(void)
{
  static int initialized;
  long ds_size;

  /* avoid double initialization */
  if (initialized)
    return 0;

  INFO(printf("DOpElib(dope_init): ask for 'dope'...\n"));

  dope_server = l4re_env_get_cap("dope");
  if (l4_is_invalid_cap(dope_server))
    {
      ERROR(printf("no 'dope' cap found!\n"));
      return -L4_ENOENT;
    }

  INFO(printf("DOpElib(dope_init): found some DOpE.\n"));

  ev_ds = l4re_util_cap_alloc();
  if (l4_is_invalid_cap(ev_ds))
    return -L4_ENOMEM;

  ev_irq = l4re_util_cap_alloc();

  if (l4_is_invalid_cap(ev_irq))
    {
      l4re_util_cap_free(ev_ds);
      return -L4_ENOMEM;
    }

  if (l4_error(l4_factory_create_irq(l4re_env()->factory, ev_irq)))
    {
      l4re_util_cap_free(ev_ds);
      l4re_util_cap_free(ev_irq);
      return -L4_ENOMEM;
    }

  if (l4_error(l4_icu_bind(dope_server, 0, ev_irq)))
    {
      l4re_util_cap_free_um(dope_server);
      l4re_util_cap_free(ev_ds);
      l4re_util_cap_free_um(ev_irq);
      return -L4_ENOMEM;
    }

  if (l4re_event_get_buffer(dope_server, ev_ds))
    {
      ERROR(printf("Did not get the event object!\n"));
      l4re_util_cap_free_um(ev_ds);
      l4re_util_cap_free_um(ev_irq);
      return -L4_ENOENT;
    }


  ds_size = l4re_ds_size(ev_ds);
  if (ds_size < 0)
    return -L4_ENOMEM;

  if (l4re_event_buffer_attach(&ev_buf, ev_ds, l4re_env()->rm))
    return -L4_ENOMEM;

  /* create mutex to make dope_cmdf and dope_cmd thread save */
  dopelib_cmdf_mutex = dopelib_mutex_create(0);
  dopelib_cmd_mutex  = dopelib_mutex_create(0);

  initialized++;
  return 0;
}


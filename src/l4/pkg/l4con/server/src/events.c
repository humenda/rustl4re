/**
 * \file   con/server/src/events.c
 * \brief  Event handling, listen for events at event server
 *
 * \date   01/03/2004
 * \author Torsten Frenzel <frenzel@os.inf.tu-dresden.de>
 */

/*
 * (c) 2003-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <stdio.h>

//#include <l4/l4con/l4con-client.h>
#include <l4/l4con/l4con.h>
//#include <l4/log/l4log.h>
#include <l4/re/c/log_printf.h>
#include <l4/sys/types.h>
#include <l4/re/errno.h>
#include <l4/util/macros.h>
#include <l4/thread/thread.h>
//#include <l4/events/events.h>

#include "events.h"

#define DEBUG_EVENTS 0

static l4_cap_idx_t con_service_id;

/**
 * \brief Event thread
 */
static void
events_init_and_wait(void *dummy)
{
#ifdef FIXME
  l4events_ch_t event_ch = L4EVENTS_EXIT_CHANNEL;
  l4events_nr_t event_nr = L4EVENTS_NO_NR;
  l4events_event_t event;
  CORBA_Environment _env = dice_default_environment;

  /* init event lib and register for event */
  l4events_init();
  l4events_register(event_ch, _CON_EVENT_THREAD_PRIORITY);

  LOGdl(DEBUG_EVENTS, "event thread up.");

  /* event loop */
  for (;;)
    {
      l4_threadid_t tid;
      int ret;
      long res;

      res = l4events_give_ack_and_receive(&event_ch, &event, &event_nr,
					  L4_IPC_NEVER, L4EVENTS_RECV_ACK);
      if (res != L4EVENTS_OK)
	{
          LOGdl(DEBUG_EVENTS, "Got bad event (result=%ld, %s)",
	      res, l4env_errstr(res));
	  continue;
	}

      tid = *(l4_threadid_t*)event.str;

      LOGdl(DEBUG_EVENTS, "Got exit event for "l4util_idfmt,
            l4util_idstr(tid));

      /* call service thread to free resources, this must be done to
       * synchronize the manipulation of loader data structures */
      ret = con_if_close_all_call(&con_service_id, &tid, &_env);
      if (ret || DICE_HAS_EXCEPTION(&_env))
        LOG_Error("handle exit event: call to service thread failed " \
                  "(ret %d, exc %d)!", ret, DICE_EXCEPTION_MAJOR(&_env));
    }
#endif
  while (1)
    {
      printf("Event thread started, but doing nothing\n");
      l4_sleep_forever();
    }
}

/**
 * \brief Init event handling
 */
void
init_events(void)
{
  static int events_init_done;

  if (!events_init_done)
    {
      l4thread_t events_tid;

      //con_service_id = l4_myself();
      events_tid = l4thread_create_long(L4THREAD_INVALID_ID,
					events_init_and_wait,
					".events", L4THREAD_INVALID_SP,
					L4THREAD_DEFAULT_SIZE,
					L4THREAD_DEFAULT_PRIO,
					0, L4THREAD_CREATE_ASYNC);
      LOGdl(DEBUG_EVENTS, "started event thread at %lx",
            l4thread_l4_id(events_tid));

      events_init_done = 1;
    }
}

/*
 * \brief   DOpE client library
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

/*** GENERIC INCLUDES ***/
#include <l4/re/c/event.h>
#include <l4/input/libinput.h>
#include <stdio.h>
#include <stdarg.h>

#include <dopelib.h>
#include "dopestd.h"
#include "sync.h"
#include "init.h"
#include "misc.h"

#define EVENT_PRESS         1
#define EVENT_RELEASE       2
#define EVENT_MOTION        3
#define EVENT_MOUSE_ENTER   4
#define EVENT_MOUSE_LEAVE   5
#define EVENT_KEY_REPEAT    6
#define EVENT_ABSMOTION     7
#define EVENT_ACTION       99


struct cb_data
{
  void (*callback)(dope_event *,void *);
  void *arg;
};

/*** UTILITY: CONVERT CALLBACK FUNCTION TO BIND ARGUMENT STRING ***/
char *dopelib_callback_to_bindarg(void (*callback)(dope_event *,void *),
                                  void *arg,
                                  char *dst_buf, int dst_len)
{
  struct cb_data *c = malloc(sizeof(struct cb_data));
  c->callback = callback;
  c->arg = arg;
  snprintf(dst_buf, dst_len, "0x%lx", (unsigned long)c);
  return dst_buf;
}


/*** INTERFACE: BIND AN EVENT TO A DOpE WIDGET ***/
L4_CV void dope_bind(const char *var, const char *event_type,
                     void (*callback)(dope_event *,void *), void *arg)
{
  char cmdbuf[256];
  char bindbuf[64];

  dopelib_mutex_lock(dopelib_cmdf_mutex);

  dopelib_callback_to_bindarg(callback, arg, bindbuf, sizeof(bindbuf));
  snprintf(cmdbuf, sizeof(cmdbuf), "%s.bind(\"%s\",%s)",
      var, event_type, bindbuf);

  dope_cmd(cmdbuf);
  dopelib_mutex_unlock(dopelib_cmdf_mutex);
}


/*** INTERFACE: BIND AN EVENT TO A DOpE WIDGET SPECIFIED AS FORMAT STRING ***/
L4_CV void dope_bindf(const char *varfmt, const char *event_type,
                void (*callback)(dope_event *,void *), void *arg,...)
{
  char varstr[512];
  va_list list;

  /* decode varargs */
  va_start(list, arg);
  vsnprintf(varstr, sizeof(varstr), varfmt, list);
  va_end(list);

  dope_bind(varstr, event_type, callback, arg);
}

L4_CV char *dope_get_bindarg(l4re_event_t *ev)
{
  if (!ev->stream_id)
    return NULL;

  struct cb_data *cb = (struct cb_data*)(ev->stream_id);
  if (!cb->callback)
    return NULL;

  return cb->arg;
}

L4_CV void dope_process_event(l4re_event_t *ev);

/*** INTERFACE: PROCESS SINGLE DOpE EVENT ***/
L4_CV void dope_process_event(l4re_event_t *ev)
{
  if (!ev->stream_id)
    return;

  struct cb_data *cb = (struct cb_data*)(ev->stream_id);
  if (!cb->callback)
    return;

  cb->callback(ev, cb->arg);
}

L4_CV void dope_deinit_app(void) {}


/*** INTERFACE: DISCONNECT FROM DOpE ***
 *
 * FIXME: wait for the finishing of the current command and block net commands
 */
L4_CV void dope_deinit(void) {}


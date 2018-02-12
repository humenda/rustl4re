/*
 * \brief   Interface of DOpE client library
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

#ifndef __DOPE_INCLUDE_DOPELIB_H_
#define __DOPE_INCLUDE_DOPELIB_H_

#include <l4/sys/compiler.h>
#include <l4/sys/linkage.h>
#include <l4/re/c/event.h>
#include <l4/re/c/event_buffer.h>
#include <l4/re/event_enums.h>

typedef l4re_event_t dope_event;

EXTERN_C_BEGIN

/*** INITIALISE DOpE LIBRARY ***/
L4_CV long  dope_init(void);


/*** DEINITIALISE DOpE LIBRARY ***/
L4_CV void  dope_deinit(void);


/*** REGISTER DOpE CLIENT APPLICATION ***
 *
 * \param appname  name of the DOpE application
 * \return         DOpE application id
 */
//L4_CV long  dope_init_app(const char *appname);


/*** UNREGISTER DOpE CLIENT APPLICATION ***
 *
 * \param app_id  DOpE application to unregister
 */
L4_CV void dope_deinit_app(void);


/*** EXECUTE DOpE COMMAND ***
 *
 * \param cmd     command to execute
 * \return        0 on success
 */
L4_CV int dope_cmd(const char *cmd);


/*** EXECUTE DOpE FORMAT STRING COMMAND ***
 *
 * \param cmdf    command to execute specified as format string
 * \return        0 on success
 */
L4_CV int dope_cmdf(const char *cmdf, ...)
  __attribute__((format (printf, 1, 2)));


/*** EXECUTE DOpE COMMAND AND REQUEST RESULT ***
 *
 * \param dst       destination buffer for storing the result string
 * \param dst_size  size of destination buffer in bytes
 * \param cmd       command to execute
 * \return          0 on success
 */
L4_CV int dope_req(char *dst, unsigned long dst_size, const char *cmd);


/*** REQUEST RESULT OF A DOpE COMMAND SPECIFIED AS FORMAT STRING ***
 *
 * \param app_id    DOpE application id
 * \param dst       destination buffer for storing the result string
 * \param dst_size  size of destination buffer in bytes
 * \param cmd       command to execute - specified as format string
 * \return          0 on success
 */
L4_CV int dope_reqf(char *dst, unsigned dst_size, const char *cmdf, ...)
  __attribute__((format (printf, 3, 4)));


/*** BIND AN EVENT TO A DOpE WIDGET ***
 *
 * \param var         widget to bind an event to
 * \param event_type  identifier for the event type
 * \param callback    callback function to be called for incoming events
 * \param arg         additional argument for the callback function
 */
L4_CV void dope_bind(const char *var, const char *event_type,
                     void (*callback)(dope_event *,void *), void *arg);


/*** BIND AN EVENT TO A DOpE WIDGET SPECIFIED AS FORMAT STRING ***
 *
 * \param varfmt      widget to bind an event to (format string)
 * \param event_type  identifier for the event type
 * \param callback    callback function to be called for incoming events
 * \param arg         additional argument for the callback function
 * \param ...         format string arguments
 */
L4_CV void dope_bindf(const char *varfmt, const char *event_type,
                      void (*callback)(dope_event *,void *), void *arg,...);


/*** ENTER DOPE EVENTLOOP ***
 */
L4_CV void dope_eventloop(void);


/*** RETURN NUMBER OF PENDING EVENTS ***
 * \return        number of pending events
 */
L4_CV int dope_events_pending(void);


/*** PROCESS ONE SINGLE DOpE EVENT ***
 *
 * This function processes exactly one DOpE event. If no event is pending, it
 * blocks until an event is available. Thus, for non-blocking operation, this
 * function should be called only if dope_events_pending was consulted before.
 */
//L4_CV void dope_process_event(void);


L4_CV char *dope_get_bindarg(l4re_event_t *ev);
L4_CV l4re_event_t *dope_event_get(void);

/*** INJECT ARTIFICIAL EVENT INTO EVENT QUEUE ***
 *
 * This function can be used to serialize events streams from multiple
 * threads into one DOpE event queue.
 */
//L4_CV void dope_inject_event(dope_event *ev,
//                             void (*callback)(dope_event *,void *), void *arg);


/*** REQUEST KEY OR BUTTON STATE ***
 *
 * \param keycode  keycode of the requested key
 * \return         1 if key is currently pressed
 */
L4_CV long dope_get_keystate(long keycode);


/*** REQUEST CURRENT ASCII KEYBOARD STATE ***
 *
 * \param keycode  keycode of the requested key
 * \return         ASCII value of the currently pressed key combination
 */
L4_CV char dope_get_ascii(long keycode);

EXTERN_C_END

#endif /* __DOPE_INCLUDE_DOPELIB_H_ */

/*
 * \brief   DDEKit timers
 * \author  Thomas Friebel <tf13@os.inf.tu-dresden.de>
 * \author  Christian Helmuth <ch12@os.inf.tu-dresden.de>
 * \author  Bjoern Doebel<doebel@os.inf.tu-dresden.de>
 * \date    2008-08-26
 */

/*
 * (c) 2006-2008 Technische Universität Dresden
 * This file is part of TUD:OS, which is distributed under the terms of the
 * GNU General Public License 2. Please see the COPYING file for details.
 */

#pragma once

#include <l4/sys/compiler.h>

EXTERN_C_BEGIN

#include <l4/dde/ddekit/thread.h>

/** \defgroup DDEKit_timer 
 *
 * Timer subsystem
 *
 * DDEKit provides a generic timer implementation that enables users
 * to execute a function with some arguments after a certain period
 * of time. DDEKit therefore starts a timer thread that executes these
 * functions and keeps track of the currently running timers.
 */

/** Add a timer event. After the absolute timeout has expired, function fn
 * is called with args as arguments.
 *
 *  \ingroup DDEKit_timer
 *
 *	\return		>=0	valid timer ID 
 *  \return		< 0	error
 */
int ddekit_add_timer(void (*fn)(void *), void *args, unsigned long timeout);

/** Delete timer with the corresponding timer id.
 *
 *  \ingroup DDEKit_timer
 */
int ddekit_del_timer(int timer);

/** Check whether a timer is pending 
 *
 *  \ingroup DDEKit_timer
 *
 * Linux needs this.
 */
int ddekit_timer_pending(int timer);

/** Initialization function, startup timer thread
 *
 *  \ingroup DDEKit_timer
 */
void ddekit_init_timers(void);

/** Get the timer thread.
 */
ddekit_thread_t *ddekit_get_timer_thread(void);

EXTERN_C_END

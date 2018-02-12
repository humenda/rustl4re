/*
 * \brief   Interface of the real-time scheduler of DOpE
 * \date    2004-04-27
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

#ifndef _DOPE_SCHEDULER_H_
#define _DOPE_SCHEDULER_H_

#include "widget.h"
#include "thread.h"

struct scheduler_services {

	/*** SCHEDULE NEW REAL-TIME WIDGET ***
	 *
	 * \param w       widget to redraw periodically
	 * \param period  period length in milliseconds
	 * \return        0 on success
	 */
	s32  (*add) (WIDGET *w, u32 period);


	/*** STOP REAL-TIME REDRAW OF SPECIFIED WIDGET ***/
	void (*remove) (WIDGET *w);


	/*** STOP REAL-TIME REDRAW OF ALL WIDGETS OF SPECIFIED APPLICATION ***/
	void (*release_app) (int app_id);


	/*** REGISTER SYNCHRONISATION MUTEX ***
	 *
	 * After each periodic redraw, the scheduler unlocks
	 * this semaphore. This mechanism can be used to provide
	 * redraw-synchronisation with a client application.
	 */
	void (*set_sync_mutex) (WIDGET *w, MUTEX *);


	/*** MAINLOOP OF DOpE ***
	 *
	 * Within the mainloop we must update real-time widgets,
	 * handle non-realtime redraw operations and pay some
	 * attention to the user.
	 */
	void (*process_mainloop) (void);
};


#endif /* _DOPE_SCHEDULER_H_ */


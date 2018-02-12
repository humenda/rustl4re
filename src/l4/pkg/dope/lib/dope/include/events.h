/*
 * \brief   DOpElib internal event interface
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

#ifndef _DOPELIB_EVENTS_H_
#define _DOPELIB_EVENTS_H_


/*** INIT EVENT QUEUE FOR SPECIFIED APPLICATION ID ***/
extern int  dopelib_init_eventqueue(int app_id);


/*** FETCH AN EVENT FROM THE EVENT QUEUE ***
 *
 * This function blocks until an event is available.
 */
extern void dopelib_wait_event(int app_id, dope_event **e_out, char **bindarg_out);

#endif /* _DOPELIB_EVENTS_H_ */

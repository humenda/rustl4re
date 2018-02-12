/*
 * \brief   DOpElib internal event listener interface
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

#ifndef _DOPELIB_LISTENER_H_
#define _DOPELIB_LISTENER_H_

#include "dopeapp-server.h"

/*** CREATE IDENTIFIER FOR CORBA_OBJECT ***/
extern void CORBA_Object_to_ident(CORBA_Object obj, char *dst, int dst_len);

/*** START UP LISTENER SERVER THREAD ***
 *
 * This function fills out the listener field of the
 * app_struct of the application with the specified
 * app_id.
 */
extern int dopelib_start_listener(long app_id);


/*** STOP LISTENER THREAD ***/
extern void dopelib_stop_listener(long app_id);


#endif /* _DOPELIB_LISTENER_H_ */

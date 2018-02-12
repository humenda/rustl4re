/*
 * \brief   DOpElib interface for handling local application data
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

#ifndef _DOPELIB_APP_STRUCT_H_
#define _DOPELIB_APP_STRUCT_H_

#define EVENT_QUEUE_SIZE 100
#define EVENT_CMD_SIZE   256

#define MAX_DOPE_CLIENTS 8

#include "dopelib.h"
#include "sync.h"

struct dopelib_app {
	dope_event event_queue[EVENT_QUEUE_SIZE];
	char bindarg_queue[EVENT_QUEUE_SIZE][256];
	char bindcmd_queue[EVENT_QUEUE_SIZE][EVENT_CMD_SIZE];
	unsigned long first;
	unsigned long last;
	long app_id;
	struct dopelib_sem *queue_sem;
	CORBA_Object_base listener;
	void *listener_ptid;         /* only used by the linux version */
};

extern struct dopelib_app *dopelib_apps[MAX_DOPE_CLIENTS];


/*** RETURN IF AN ID IS VALID ***/
static inline int valid_app_id(int id) {

	/* test if id is in valid range and an app struct exists */
	if ((id < 0) || (id >= MAX_DOPE_CLIENTS) || !dopelib_apps[id])
		return 0;

	return 1;
}


/*** RETURN APP STRUCT FOR A SPECIFIED ID ***/
static inline struct dopelib_app *get_app(int id) {

	/* if id is valid, return the corresponding app struct */
	return valid_app_id(id) ? dopelib_apps[id] : NULL;
}

#endif /* _DOPELIB_APP_STRUCT_H_ */

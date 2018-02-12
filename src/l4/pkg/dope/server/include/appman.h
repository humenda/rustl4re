/*
 * \brief   Interface of DOpE application manager module
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

#ifndef _DOPE_APPMAN_H_
#define _DOPE_APPMAN_H_

#include "hashtab.h"
#include "thread.h"
#include "scope.h"

#define MAX_APPS          64    /* maximum amount of DOpE clients          */

struct appman_services {
	s32      (*reg_app)          (const char *app_name);
	s32      (*unreg_app)        (u32 app_id);
	void     (*set_rootscope)    (u32 app_id, SCOPE *rootscope);
	SCOPE   *(*get_rootscope)    (u32 app_id);
	void     (*reg_listener)     (s32 app_id, void *listener);
	void     (*reg_list_thread)  (s32 app_id, THREAD *);
	void    *(*get_listener)     (s32 app_id);
	char    *(*get_app_name)     (s32 app_id);
	void     (*reg_app_thread)   (s32 app_id, THREAD *app_thread);
	THREAD  *(*get_app_thread)   (s32 app_id);
	s32      (*app_id_of_thread) (THREAD *app_thread);
	s32      (*app_id_of_name)   (char *app_name);
	void     (*lock)             (s32 app_id);
	void     (*unlock)           (s32 app_id);
};


#endif /* _DOPE_APPMAN_H_ */


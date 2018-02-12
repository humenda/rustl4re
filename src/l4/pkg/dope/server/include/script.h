/*
 * \brief   Interface of the DOpE command interpreter
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

#ifndef _DOPE_SCRIPT_H_
#define _DOPE_SCRIPT_H_

struct widtype;
struct script_services {
	void *(*reg_widget_type)   (char *widtype_name, void *(*create_func)(void));
	void  (*reg_widget_method) (struct widtype *, char *desc, void *methadr);
	void  (*reg_widget_attrib) (struct widtype *, char *desc, void *get, void *set, void *update);
	int   (*exec_command)      (u32 app_id, const char *cmd, char *dst, int dst_len);
};


#endif /* _DOPE_SCRIPT_H_ */


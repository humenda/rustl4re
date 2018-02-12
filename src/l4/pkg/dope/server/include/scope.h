/*
 * \brief   Interface of DOpE Scope widget module
 * \date    2003-06-11
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

#ifndef _DOPE_SCOPE_H_
#define _DOPE_SCOPE_H_

#include "hashtab.h"
#include "widget.h"

struct scope_methods;
struct scope_data;

#define SCOPE struct scope

struct scope {
	struct widget_methods *gen;
	struct scope_methods  *scope;
	struct widget_data    *wd;
	struct scope_data     *sd;
};

struct scope_methods {
	int     (*set_var)      (SCOPE *s, char *type, char *name, int len, WIDGET *value);
	WIDGET *(*get_var)      (SCOPE *s, char *name, int len);
	char   *(*get_vartype)  (SCOPE *s, char *name, int len);
	SCOPE  *(*get_subscope) (SCOPE *s, char *name, int len);
};

struct scope_services {
	SCOPE *(*create) (void);
};


#endif /* _DOPE_SCOPE_H_ */

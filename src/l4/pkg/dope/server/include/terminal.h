/*
 * \brief   Interface of DOpE Terminal widget module
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

#ifndef _DOPE_TERMINAL_H_
#define _DOPE_TERMINAL_H_

#include "widget.h"

struct terminal_methods;
struct terminal_data;

#define TERMINAL struct terminal

struct terminal {
	struct widget_methods   *gen;
	struct terminal_methods *term;
	struct widget_data      *wd;
	struct terminal_data    *td;
};

struct terminal_methods {
	void (*print) (TERMINAL *,char *txt);
};

struct terminal_services {
	TERMINAL *(*create) (void);
};


#endif /* _DOPE_TERMINAL_H_ */


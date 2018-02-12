/*
 * \brief   Interface of DOpE Label widget module
 * \date    2003-05-15
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

#ifndef _DOPE_LABEL_H_
#define _DOPE_LABEL_H_

#include "widget.h"
#include "variable.h"

struct label_methods;
struct label_data;

#define LABEL struct label

struct label {
	struct widget_methods *gen;
	struct label_methods  *lab;
	struct widget_data    *wd;
	struct label_data     *ld;
};

struct label_methods {
	void      (*set_text)  (LABEL *, char *new_txt);
	char     *(*get_text)  (LABEL *);
	void      (*set_font)  (LABEL *, char *new_fontname);
	char     *(*get_font)  (LABEL *);
	void      (*set_var)   (LABEL *, VARIABLE *v);
	VARIABLE *(*get_var)   (LABEL *);
};

struct label_services {
	LABEL *(*create) (void);
};


#endif /* _DOPE_LABEL_H_ */

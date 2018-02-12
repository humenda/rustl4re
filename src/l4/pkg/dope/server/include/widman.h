/*
 * \brief   Interface of the widget manager module of DOpE
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

#ifndef _DOPE_WIDMAN_H_
#define _DOPE_WIDMAN_H_

#include "widget.h"

struct widman_services {
	void (*default_widget_data)    (struct widget_data *);
	void (*default_widget_methods) (struct widget_methods *);
	void (*build_script_lang)      (void *widtype,struct widget_methods *);
};


#endif /* _DOPE_WIDMAN_H_ */


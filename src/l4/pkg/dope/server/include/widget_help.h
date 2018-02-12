
/*
 * \brief   Widget type default includes
 * \date    2004-02-01
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

#ifndef _DOPE_WIDGET_HELP_H_
#define _DOPE_WIDGET_HELP_H_

#include "widget.h"


/*** ALLOCATE WIDGET STRUCTURE OF SPECIFIED TYPE ***/
#define ALLOC_WIDGET(widtype)                          \
	(widtype *)zalloc(sizeof(widtype)                   \
	                + sizeof(struct widget_data)         \
	                + sizeof(widtype ## _data));


/*** INITIALIZE WIDGET STRUCTURE ***
 *
 * \param wid           name of widget struct to initialize
 * \param widtype       type of widget struct
 * \param spec_methods  widget specific function table
 */
#define SET_WIDGET_DEFAULTS(wid, widtype, spec_methods) {                         \
	if (!wid) {                                                                    \
		ERROR(printf("Error: out of memory. Widget allocation failed.\n"));         \
		return NULL;                                                                 \
	}                                                                                 \
	((struct widget *)wid)->gen  = &gen_methods;                                       \
	((struct widget *)wid)->spec = spec_methods;                                        \
	((struct widget *)wid)->wd   = (struct widget_data *)((adr)wid + sizeof(widtype));   \
	((struct widget *)wid)->sd   = (widtype  ##  _data *)((adr)wid->wd                    \
	                                                   + sizeof(struct widget_data));      \
	widman->default_widget_data(wid->wd);                                                   \
}

#endif /* _DOPE_WIDGET_HELP_H_ */

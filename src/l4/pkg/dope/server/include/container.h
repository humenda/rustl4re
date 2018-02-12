/*
 * \brief   Interface of DOpE Container widget module
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

#ifndef _DOPE_CONTAINER_H_
#define _DOPE_CONTAINER_H_

#include "widget.h"

struct container_methods;
struct container_data;

#define CONTAINER struct container

struct container {
	struct widget_methods    *gen;
	struct container_methods *cont;
	struct widget_data       *wd;
	struct container_data    *cd;
};

struct container_methods {
	void     (*add)         (CONTAINER *,WIDGETARG *new_element);
	void     (*remove)      (CONTAINER *,WIDGETARG *element);
	WIDGET  *(*get_content) (CONTAINER *);
};

struct container_services {
	CONTAINER *(*create) (void);
};

#endif /* _DOPE_CONTAINER_H_ */

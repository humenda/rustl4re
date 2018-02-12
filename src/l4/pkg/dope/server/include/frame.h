/*
 * \brief   Interface of DOpE Frame widget module
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

#ifndef _DOPE_FRAME_H_
#define _DOPE_FRAME_H_

#include "widget.h"

struct frame_methods;
struct frame_data;

#define FRAME struct frame

struct frame {
	struct widget_methods *gen;
	struct frame_methods  *frame;
	struct widget_data    *wd;
	struct frame_data     *fd;
};

struct frame_services {
	FRAME *(*create) (void);
};


#endif /* _DOPE_FRAME_H_ */


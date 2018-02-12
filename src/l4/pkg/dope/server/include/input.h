/*
 * \brief   Interface of input event layer of DOpE
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

#ifndef _DOPE_INPUT_H_
#define _DOPE_INPUT_H_

#include "widget.h"
#include "event.h"

struct input_services {
	int     (*get_event)    (EVENT *e);
};


#endif /* _DOPE_INPUT_H_ */

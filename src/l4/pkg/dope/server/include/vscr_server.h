/*
 * \brief   Interface of VScreen server module
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

#ifndef _DOPE_VSCR_SERVER_H_
#define _DOPE_VSCR_SERVER_H_

#include "thread.h"
#include "vscreen.h"

struct vscr_server_services {
	int (*start) (THREAD *, VSCREEN *);
};


#endif /* _DOPE_VSCR_SERVER_H_ */

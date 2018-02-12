/*
 * \brief   Generic utility function of the DOpE VScreen library
 * \date    2003-08-05
 * \author  Norman Feske <nf2@inf.tu-dresden.de>
 */

/*
 * Copyright (C) 2002-2003  Norman Feske  <nf2@os.inf.tu-dresden.de>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the DOpE package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */

/*** DOpE INCLUDES ***/
#include "dopelib.h"
#include "vscreen.h"


/*** INTERFACE: MAP FRAMEBUFFER OF VSCREEN WIDGET GIVEN BY ITS NAME ***/
void *vscr_get_fb(int app_id, const char *vscr) {
	char retbuf[256];
	dope_reqf(app_id, retbuf, 256, "%s.map()", vscr);
	return vscr_map_smb(retbuf);
}


/*** INTERFACE: GET VSCREEN SERVER OF THE SPECIFIED WIDGET ***/
void *vscr_get_server_id(int app_id, const char *vscr) {
	char retbuf[256];
	dope_reqf(app_id, retbuf, 256, "%s.getserver()", vscr);
	return vscr_connect_server(retbuf);
}

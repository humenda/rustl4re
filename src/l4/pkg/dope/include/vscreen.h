/*
 * \brief   Interface of VScreen library
 * \date    2002-11-25
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

#pragma once

/******************************************
 * HANDLE SHARED MEMORY BUFFER OF VSCREEN *
 ******************************************/

#include <l4/sys/compiler.h>

EXTERN_C_BEGIN

/*** RETURN LOCAL ADDRESS OF SPECIFIED SHARED MEMORY BLOCK ***
 *
 * \param smb_ident  identifier string of the shared memory block
 * \return           address of the smb block in local address space
 *
 * The format of the smb_ident string is platform dependent. The
 * result of the VScreen widget's map function can directly be passed
 * into this function. (as done by vscr_get_fb)
 */
L4_CV void *vscr_map_smb(char *smb_ident);


/*** RETURN LOCAL ADDRESS OF THE FRAMEBUFFER OF THE SPECIFIED VSCREEN ***
 *
 * \param vscr_name  name of the VScreen widget
 * \return           address of the VScreen buffer in the local address space
 */
L4_CV void *dope_vscr_get_fb(const char *vscr_name);


/*** RELEASE VSCREEN BUFFER FROM LOCAL ADDRESS SPACE ***
 *
 * \param fb_adr   start address of the vscreen buffer
 * \return         0 on success
 */
L4_CV int vscr_free_fb(void *fb_adr);


/*************************
 * HANDLE VSCREEN SERVER *
 *************************/

/*** CONNECT TO SPECIFIED VSCREEN WIDGET SERVER ***
 *
 * \param vscr_ident  identifier of the widget's server
 * \return            id to be used for the vscr_server-functions
 */
L4_CV void *vscr_connect_server(char *vscr_ident);


/*** GET SERVER ID OF THE SPECIFIED VSCREEN WIDGET ***
 *
 * \param app_id     DOpE application id to which the VScreen widget belongs
 * \param vscr_name  name of the VScreen widget
 * \return           id to be used for the vscr_server-functions
 */
L4_CV void *vscr_get_server_id(int app_id, const char *vscr_name);


/*** CLOSE CONNECTION TO VSCREEN SERVER ***
 *
 * \param vscr_server_id  id of the vscreen widget server to disconnect from
 */
L4_CV void  vscr_release_server_id(void *vscr_server_id);


/*** WAIT FOR SYNCHRONISATION ***
 *
 * This function waits until the current drawing period of this
 * widget is finished.
 */
L4_CV void  vscr_server_waitsync(void *vscr_server_id);


/*** REFRESH RECTANGULAR WIDGET AREA ASYNCHRONOUSLY ***
 *
 * This function refreshes the specified area of the VScreen widget
 * in a best-efford way. The advantage of this function over a
 * dope_cmd call of vscr.refresh() is its deterministic execution
 * time and its nonblocking way of operation.
 */
L4_CV void  vscr_server_refresh(void *vscr_server_id, int x, int y, int w, int h);

EXTERN_C_END

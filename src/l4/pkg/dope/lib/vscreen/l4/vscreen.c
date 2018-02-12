/*
 * \brief   L4 specific DOpE VScreen library
 * \date    2002-11-13
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

/*** GENERAL INCLUDES ***/
#include <stdio.h>
#include <string.h>

/*** L4 SPECIFIC INCLUDES ***/
#include <l4/sys/types.h>
#include <l4/util/util.h>
#include <l4/dope/vscreen.h>

#define MAX_VSCREENS 32
#define DEBUG(x) /* x */


struct vscr {
	char          *name;
	l4_threadid_t  tid;
} vscreens[MAX_VSCREENS];


/*** ALLOCATE NEW VSCREEN ID ***/
static int get_new_index(void) {
	int i;
		
	/* find free vscr id */
	for (i=0;i<MAX_VSCREENS;i++) {
		if (!vscreens[i].name) break;
	}

	if (i>=MAX_VSCREENS) return -1;
	return i;
}


/*** UTILITY: CHECK IF A GIVEN VSCR ID IS VALID ***/
static int valid_index(int id) {

	if ((id<0) || (id>=MAX_VSCREENS)) return 0;
	if (!vscreens[id].name) return 0;
	return 1;
}

#if 0

/*** UTILITY: RELEASE VSCREEN SERVER ID ***/
static void release_index(int i) {
	if (!valid_index(i)) return;
	vscreens[i].name = NULL;    
}


/*** INTERFACE: ESTABLISH CONNECTION TO VSCREEN SERVER ***/
void *vscr_connect_server(char *ident) {
	long i;

	if (ident) DEBUG(printf("libVScr(get_id): ident = %s\n",ident));
	if ((i = get_new_index()) <0) return NULL;
	if (!strcmp("<undefined>", ident)) return NULL;

	vscreens[i].name = ident;
	
	/* request thread id of VScreen-server using its identifier */
	if (names_waitfor_name(ident, &vscreens[i].tid, 50000) == 0) {
		printf("libVScr(get_id): VScreen-server not found!\n");
		return NULL;
	}

	return (void *)i+1;
}


/*** INTERFACE: DISCONNECT FROM VSCREEN SERVER ***/
void vscr_release_server_id(void *id) {
	long i = ((long)id) - 1;

	if (!valid_index(i)) return;

	/* !!! unmap vscreen memory !!! */
	release_index(i);
}
#endif

/*** UTILITY: CONVERT STRING WITH HEX NUMBER TO LONG INT ***/
static unsigned long hex2u32(char *s) {
	int i;
	unsigned long result=0;
	for (i=0;i<8;i++,s++) {
		if (!(*s)) return result;
		result = result*16 + (*s & 0xf);
		if (*s > '9') result += 9;
	}
	return result;
}


/*** LIB-INTERNAL: MAP SHARED MEMORY BLOCK INTO LOCAL ADDRESS SPACE ***/
void *vscr_map_smb(char *smb_ident)
{
  l4dm_dataspace_t ds;
  long smb_size = 0;
  void *fb_adr;
  unsigned int res;

  DEBUG(printf("libVScreen(get_fb): smb_ident = %s\n",smb_ident));

  l4_cap_idx_t x = strtoul(&buf[3], NULL, 0);
  _fb_ds = L4::Cap<L4Re::Dataspace>(x);
  printf("fb_ds = %lx\n", _fb_ds.cap());


  //ds.manager.raw = hex2u32(smb_ident +  7);
  //ds.id          = hex2u32(smb_ident + 24);
  //smb_size       = hex2u32(smb_ident + 40);

  //DEBUG(printf("libVScreen(get_fb): DS = " l4util_idfmt " id = 0x%x "
//	"fb-size = %dKB\n",
//	(int)l4util_idstr(ds.manager), (int)ds.id, (int)(smb_size >> 10)));

  if ((res = l4rm_attach(&ds, smb_size, 0, L4DM_RW, &fb_adr))) {
    printf("libVScr(get_fb): l4rm_attach failed (err: %s(%d))\n",
	l4env_errstr(res), res);
    return 0;
  }

  DEBUG(printf("libVScr(get_fb): fb_adr = 0x%lx\n", (unsigned long)fb_adr));
  return fb_adr;
}


/*** INTERFACE: WAIT FOR END OF CURRENT REDRAW OPERATION ***/
void vscr_server_waitsync(void *id) {
	CORBA_Environment env = dice_default_environment;
	long i = ((long)id) - 1;
	if (!valid_index(i)) return;
	dope_vscr_waitsync_call(&vscreens[i].tid,&env);
	l4_thread_switch(L4_NIL_ID);
}


/*** INTERFACE: ASYNCHRONOUS REFRESH OF A VSCREEN REGION ***/
void vscr_server_refresh(void *id, int x, int y, int w, int h) {
	CORBA_Environment env = dice_default_environment;
	long i = ((long)id) - 1;
	if (!valid_index(i)) return;
	dope_vscr_refresh_call(&vscreens[i].tid, x, y, w, h, &env);
	l4_thread_switch(L4_NIL_ID);
}


/*** INTERFACE: UNMAP FRAME BUFFER OF VSCREEN WIDGET ***/
int vscr_free_fb(void *fb) {
	return l4rm_detach(fb);
}


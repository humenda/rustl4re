/*
 * \brief   Linux specific DOpE VScreen library
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

#include <vscreen.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#define MAX_VSCREENS 32

struct vscr_region {
	void *base;
	int  allocated;
	long size;
} vscr_regions[MAX_VSCREENS];


/*** UTILITY: ALLOCATE NEW VSCREEN REGION STRUCT ***/
static struct vscr_region *alloc_vscr_region(void) {
	int i;
	for (i=0; i<MAX_VSCREENS; i++) {
		if (!vscr_regions[i].allocated) {
			vscr_regions[i].allocated = 1;
			return &vscr_regions[i];
		}
	}
	return NULL;
}


/*** UTILITY: FREE VSCREEN REGION STRUCT ***/
static void free_vscr_region(struct vscr_region *r) {
	int i;
	for (i=0; i<MAX_VSCREENS; i++) {

		/* if specified region is valid, mark it as free */
		if (r == &vscr_regions[i]) vscr_regions[i].allocated = 0;
	}
}


/*** UTILITY: GET VSCREEN REGION STRUCT BY A SPECIFIED BASE ADDRESS ***/
static struct vscr_region *get_vscr_region(void *base) {
	int i;
	for (i=0; i<MAX_VSCREENS; i++) {
		if (vscr_regions[i].base == base) return &vscr_regions[i];
	}
	return NULL;
}


/*** UTILITY: CONVERT ASCII HEX NUMBER TO UNSIGNED LONG ***/
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


/*** INTERFACE: MAP SHARED MEMORY BUFFER INTO LOCAL ADDRESS SPACE ***/
void *vscr_map_smb(char *smb_ident) {
	int fh;
	struct vscr_region *vr = alloc_vscr_region();
	
	if (!vr) {
		printf("libVScreen(map_smb): maximum number of %d vscreens reached\n", MAX_VSCREENS);
		return NULL;
	}
	
	fh = open(smb_ident+21, O_RDWR);
	vr->size = hex2u32(smb_ident+7);
	vr->base = mmap(NULL, vr->size, PROT_READ | PROT_WRITE,
	           MAP_SHARED, fh, 0);
	printf("libVScreen(map_smb): mmap file %s to addr %p\n",
	       smb_ident + 21, vr->base);
	if ((int)vr->base == -1) return NULL;
	return vr->base;
}


/*** INTERFACE: RELEASE VSCREEN BUFFER FROM LOCAL ADDRESS SPACE ***/
int vscr_free_fb(void *fb_adr) {
	int ret;
	struct vscr_region *vr = get_vscr_region(fb_adr);
	if (!vr) {
		printf("libVScreen(free_fb): illegal vscreen base address\n");
		return -1;
	}
	ret = munmap(vr->base, vr->size);
	free_vscr_region(vr);
	return ret;
}


void vscr_server_waitsync(void *id) {
	usleep(40*1000);
}

void vscr_server_refresh(void *id, int x, int y, int w, int h) { }

void *vscr_connect_server(char *ident) { return NULL; }

void vscr_release_server_id(void *id) {}

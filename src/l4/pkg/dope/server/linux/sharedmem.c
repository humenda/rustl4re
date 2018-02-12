/*
 * \brief   DOpE shared memory management module
 * \date    2002-02-04
 * \author  Norman Feske <no@atari.org>
 *
 * This component provides an abstraction for handling
 * shared memory.
 */

/*
 * Copyright (C) 2002-2004  Norman Feske  <nf2@os.inf.tu-dresden.de>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the DOpE package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */

#include <stdio.h>

#include "dopestd.h"
#include "module.h"
#include "thread.h"
#include "sharedmem.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>


static int ident_cnt = 0;

struct shared_memory {
	int   fh;
	s32   size;
	char  fname[64];
	void *addr;
};

int init_sharedmem(struct dope_services *d);


/*************************
 *** SERVICE FUNCTIONS ***
 *************************/


/*** ALLOCATE SHARED MEMORY BLOCK OF SPECIFIED SIZE ***/
static SHAREDMEM *shm_alloc(s32 size) {
	SHAREDMEM *new = malloc(sizeof(SHAREDMEM));
	if (!new) {
		ERROR(printf("SharedMemory(alloc): out of memory.\n"));
		return NULL;
	}

	/* open file */
	sprintf(new->fname, "/tmp/dopevscr%d", ident_cnt++);
	new->fh = open(new->fname, O_CREAT | O_RDWR | O_TRUNC, S_IRWXU);
	new->size = size;
	ftruncate(new->fh, new->size);
	new->addr = mmap(NULL, new->size, PROT_READ | PROT_WRITE,
	                 MAP_SHARED, new->fh, 0);
	printf("SharedMem(alloc): mmap file %s to addr %x\n", new->fname, (int)new->addr);
	return new;
}


/*** FREE SHARED MEMORY BLOCK ***/
static void shm_destroy(SHAREDMEM *sm) {
	if (!sm) return;
	munmap(sm->addr, sm->size);
	close(sm->fh);
	free(sm);
}


/*** RETURN THE ADRESS OF THE SHARED MEMORY BLOCK ***/
static void *shm_get_adr(SHAREDMEM *sm) {
	if (!sm) return NULL;
	return sm->addr;
}


/*** GENERATE A GLOBAL IDENTIFIER FOR THE SPECIFIED SHARED MEMORY BLOCK ***/
static void shm_get_ident(SHAREDMEM *sm, char *dst) {
	if (!sm) return;
	sprintf(dst, "size=0x%08X file=%s", (int)sm->size, sm->fname);
}


/*** SHARE MEMORY BLOCK TO ANOTHER THREAD ***/
static s32 shm_share(SHAREDMEM *sm, THREAD *dst_thread) {
	return 0;
}



/****************************************
 *** SERVICE STRUCTURE OF THIS MODULE ***
 ****************************************/

static struct sharedmem_services sharedmem = {
	shm_alloc,
	shm_destroy,
	shm_get_adr,
	shm_get_ident,
	shm_share,
};



/**************************
 *** MODULE ENTRY POINT ***
 **************************/

int init_sharedmem(struct dope_services *d) {

	d->register_module("SharedMemory 1.0",&sharedmem);
	return 1;
}

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

#include <l4/re/c/util/cap_alloc.h>
#include <l4/re/c/dataspace.h>
#include <l4/re/c/rm.h>
#include <l4/re/c/mem_alloc.h>
#include <l4/sys/types.h>


struct shared_memory {
	l4re_ds_t   ds;
	s32         size;
	void       *addr;
};

int init_sharedmem(struct dope_services *d);


/*************************
 *** SERVICE FUNCTIONS ***
 *************************/


/*** ALLOCATE SHARED MEMORY BLOCK OF SPECIFIED SIZE ***/
static SHAREDMEM *shm_alloc(long size)
{
  SHAREDMEM *n = malloc(sizeof(SHAREDMEM));
  if (!n) {
    ERROR(printf("SharedMemory(alloc): out of memory.\n"));
    return NULL;
  }

  n->ds = l4re_util_cap_alloc();
  if (l4_is_invalid_cap(n->ds))
    return NULL;

  if (l4re_ma_alloc(size, n->ds, 0))
    {
      l4re_util_cap_free(n->ds);
      return NULL;
    }

  n->addr = 0;
  if (l4re_rm_attach(&n->addr, size, L4RE_RM_SEARCH_ADDR | L4RE_RM_EAGER_MAP, n->ds, 0,
                     L4_PAGESHIFT))
    {
      l4re_util_cap_free_um(n->ds);
      return NULL;
    }

  n->size = size;
  //printf("SharedMem(alloc): size=%lx\n", size);
  return n;
}


/*** FREE SHARED MEMORY BLOCK ***/
static void shm_destroy(SHAREDMEM *sm)
{
  printf("%s %d\n", __func__, __LINE__); 

  if (!sm)
    return;

  l4re_rm_detach(sm->addr);
  l4re_util_cap_free_um(sm->ds);
  free(sm);
}


/*** RETURN THE ADRESS OF THE SHARED MEMORY BLOCK ***/
static void *shm_get_adr(SHAREDMEM *sm)
{
  //printf("%s %d\n", __func__, __LINE__); 
  if (!sm) return NULL;
  //printf("SharedMem(get_adr): address = %p\n", sm->addr);
  return sm->addr;
}


/*** GENERATE A GLOBAL IDENTIFIER FOR THE SPECIFIED SHARED MEMORY BLOCK ***/
static void shm_get_ident(SHAREDMEM *sm, char *dst)
{
  if (!sm)
    return;
  sprintf(dst, "ds=0x%08lx", sm->ds);
}


/*** SHARE MEMORY BLOCK TO ANOTHER THREAD ***/
static s32 shm_share(SHAREDMEM *sm, THREAD *dst_thread)
{
(void)dst_thread;
  //int res;
  if (!sm)
    return -1;

 // if ((res = l4dm_check_rights(&sm->ds, L4DM_RW)) != 0)
 //   return -1;

  INFO(printf("SharedMem(share): share to %lx\n", dst_thread->tid));

  //if ((res = l4dm_share(&sm->ds, dst_thread->tid, L4DM_RW)) != 0)
  //  return -1;
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
	d->register_module("SharedMemory 1.0", &sharedmem);
	return 1;
}

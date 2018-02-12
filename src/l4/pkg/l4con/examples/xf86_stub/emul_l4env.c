/*!
 * \file   con/examples/xf86_stub/emul_l4rm.c
 * \brief  L4Env emulation functions to be used for L4Linux programs.
 *
 * \date   06/19/2001
 * \author Jork Loeser <jork.loeser@inf.tu-dresden.de>
 *
 * This file contains the rm_* emulation functions, because we cannot run
 * the region mapper with Linux (yes, we could. Somehow.).
 *
 * !!! We must _not_ use -ll4env-l4x.p here since we need a version !!!
 * !!! where system calls are wrapped through X (mmap => xf86mmap;  !!!
 * !!! see CPPFLAGS_emul_l4env.c)                                   !!!
 */

/*
 * Copyright (c) 2003 by Technische Universität Dresden, Germany
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * TECHNISCHE UNIVERSITÃ„T DRESDEN BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef XORG73
#include "xf86_ansic.h"
#include "xf86_libc.h"
#else
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>
#endif

#include <l4/sys/syscalls.h>
#include <l4/sys/consts.h>
#include <l4/l4rm/l4rm.h>
#include <l4/dm_mem/dm_mem.h>
#include <l4/env/env.h>

/***************************************************************************
 * l4rm stuff
 **************************************************************************/

struct vm_area_struct
{
    void *                addr;
    l4_size_t	          size;
    l4_offs_t             offs;
    l4dm_dataspace_t      ds;
    struct vm_area_struct *next;
};

static int fd = 0;
static struct vm_area_struct *vm_areas;

static struct vm_area_struct *reserve_virtual_area(l4_size_t size)
{
  struct vm_area_struct *vm;
  void *addr;

  if (!fd && (fd = open("/dev/zero", O_RDONLY)) < 0)
    return NULL;

  if ((addr = mmap(0, size, PROT_NONE, MAP_SHARED, fd, 0)) == MAP_FAILED)
    return NULL;

  if (!(vm = malloc(sizeof(struct vm_area_struct))))
    return NULL;

  vm->addr = addr;
  vm->size = size;

  /* add new area to vm area list */
  vm->next = vm_areas;
  vm_areas = vm;

  /* Until we touch the pages, they are not mapped. And if we touch
     them, we get a SIGSEGV. */
  return vm;
}

static void free_virtual_area(struct vm_area_struct *vm)
{
  munmap(vm->addr, vm->size);
  free(vm);
}

/**
 * \brief Find area which contains \a addr
 */
static struct vm_area_struct *find_virtual_area(l4_addr_t addr)
{
  struct vm_area_struct * vm;

  vm = vm_areas;
  while (vm != NULL &&
         (addr <   (l4_addr_t)vm->addr ||
          addr >= ((l4_addr_t)vm->addr + vm->size)))
    vm = vm->next;

  return vm;
}

/**
 * \brief Find area which contains \a addr and remove from vm area list
 */
static struct vm_area_struct *remove_virtual_area(l4_addr_t addr)
{
  struct vm_area_struct * vm, * tmp;

  vm = vm_areas;
  tmp = NULL;
  while (vm != NULL &&
         (addr <  (l4_addr_t)vm->addr &&
          addr >= (l4_addr_t)vm->addr + vm->size))
    {
      tmp = vm;
      vm = vm->next;
    }

  if (vm == NULL)
    /* nothing found */
    return NULL;

  /* remove */
  if (tmp == NULL)
    /* remove first list element */
    vm_areas = vm_areas->next;
  else
    tmp->next = vm->next;

  return vm;
}

/*!\brief Attach dataspace.
 *
 * Find an unused map region and attach dataspace area
 * (ds_offs, ds_offs + size) to that region.
 *
 * We use get_vm_area() to find a portion of free memory. Then, we establish
 * the mapping by issuing dataspace manager calls.
 *
 * \note We ignore the flags.
 */
int
l4rm_do_attach(const l4dm_dataspace_t * ds, l4_uint32_t area, l4_addr_t * addr, 
               l4_size_t size, l4_offs_t ds_offs, l4_uint32_t flags)
{
  struct vm_area_struct *vm;
  unsigned off;
  int err;
  l4_addr_t a,fpage_addr;
  l4_size_t fpage_size;

  /* align size */
  size = l4_round_page(size);

  /* sanity checks */
  if (l4dm_is_invalid_ds(*ds))
    return -EINVAL;
                
  if (!(vm = reserve_virtual_area(size)))
    return -ENOMEM;
  
  *addr = (l4_addr_t)vm->addr;

  vm->ds = *ds;
  vm->offs = ds_offs;

  a = (l4_addr_t)vm->addr;
  for (off = 0; off < size; off += L4_PAGESIZE)
    {
      err = l4dm_map_pages(ds,ds_offs, L4_PAGESIZE, l4_trunc_page(a + off),
			   L4_LOG2_PAGESIZE,0,L4DM_RW,&fpage_addr,&fpage_size);
      if (err < 0)
	{
	  int i;
	  for(i = 0; i < off; i += L4_PAGESIZE)
	    {
	      l4_fpage_unmap(l4_fpage(l4_trunc_page(a + i),
				      L4_LOG2_PAGESIZE, 0, 0),
			     L4_FP_FLUSH_PAGE); 
	    }

	  return -ENOMEM;
	}

      ds_offs+=L4_PAGESIZE;
    }
  
  return 0;
}

/*!\brief Pretend to detach a dataspace from a region.
 *
 * Actually, this function flushes the pages and frees the vm-area.
 */
int
l4rm_detach(const void * addr)
{
  struct vm_area_struct *vm;
  int off;

  if (!(vm = remove_virtual_area((l4_addr_t)addr)))
    return -EINVAL;

  for (off = 0; off < vm->size; off += L4_PAGESIZE)
    {
      l4_fpage_unmap(l4_fpage(((l4_addr_t)vm->addr + off) & L4_PAGEMASK,
                              L4_LOG2_PAGESIZE, 0, 0),
                     L4_FP_FLUSH_PAGE);
    }

  free_virtual_area(vm);
  return 0;
}

/*!\brief Pretend to reserve an area
 *
 * This function actually does nothing.
 *
 * \note This means, areas are comletely ignored!
 */
int l4rm_do_reserve(l4_addr_t *addr, l4_size_t size, l4_uint32_t flags,
                    l4_uint32_t *area)
{
  *area = 0;
  return 0;
}

/**
 * \brief Lookup VM address
 */
int l4rm_lookup(const void * addr, l4_addr_t * map_addr, l4_size_t * map_size,
                l4dm_dataspace_t * ds, l4_offs_t * offset, l4_threadid_t * pager)
{
  struct vm_area_struct * vm;
  l4_addr_t a = (l4_addr_t)addr;

  if (!(vm = find_virtual_area(a)))
    return -EINVAL;

  *ds       = vm->ds;
  *offset   = (a - (l4_addr_t)vm->addr) + vm->offs;
  *map_addr = (l4_addr_t)vm->addr;
  *map_size = vm->size;

  return L4RM_REGION_DATASPACE;
}

/* more dummies */
int 
l4rm_init(int have_l4env, l4rm_vm_range_t used[], int num_used)
{
  return 0;
}

void
l4rm_service_loop(void)
{
  for (;;);
}

void
l4rm_show_region_list(void)
{
}

l4_threadid_t
l4env_get_default_dsm(void)
{
  return L4_INVALID_ID;
}

void LOG_logL(const char*file, int line, const char*function, const char*format,...);
void LOG_logL(const char*file, int line, const char*function, const char*format,...)
{
}

int LOG_printf(const char *format, ...);
int LOG_printf(const char *format, ...)
{
  return 0;
}

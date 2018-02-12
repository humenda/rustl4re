/**
 * \file
 * \brief  Example of coarse grained memory allocation, in C.
 */
/*
 * (c) 2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/re/c/mem_alloc.h>
#include <l4/re/c/rm.h>
#include <l4/re/c/util/cap_alloc.h>
#include <l4/sys/err.h>
#include <stdio.h>
#include <string.h>

/**
 * \brief Allocate memory, given in bytes in the granularity of pages.
 *
 * \param size_in_bytes   Size to allocate, in bytes, will be truncates to
 *                          whole pages (L4_PAGESIZE).
 * \param flags           Flags to control memory allocation:
 *                          L4RE_MA_CONTINUOUS:  Physically continuous memory
 *                          L4RE_MA_PINNED:      Pinned memory
 *                          L4RE_MA_SUPER_PAGES: Use big pages
 * \retval virt_addr      Virtual address the memory is accessible under,
 *                          undefined if return code != 0
 *
 * \return 0 on success, error code otherwise
 */
static int allocate_mem(unsigned long size_in_bytes, unsigned long flags,
                        void **virt_addr)
{
  int r;
  l4re_ds_t ds;

  /* Allocate a free capability index for our data space */
  ds = l4re_util_cap_alloc();
  if (l4_is_invalid_cap(ds))
    return -L4_ENOMEM;

  size_in_bytes = l4_trunc_page(size_in_bytes);

  /* Allocate memory via a dataspace */
  if ((r = l4re_ma_alloc(size_in_bytes, ds, flags)))
    return r;

  /* Make the dataspace visible in our address space */
  *virt_addr = 0;
  if ((r = l4re_rm_attach(virt_addr, size_in_bytes,
                          L4RE_RM_SEARCH_ADDR, ds, 0,
                          flags & L4RE_MA_SUPER_PAGES
                             ? L4_SUPERPAGESHIFT : L4_PAGESHIFT)))
    {
      /* Free dataspace again */
      l4re_util_cap_free_um(ds);
      return r;
    }

  /* Done, virtual address is in virt_addr */
  return 0;
}

/**
 * \brief Free previously allocated memory.
 *
 * \param virt_addr    Virtual address return by allocate_mem
 *
 * \return 0 on success, error code otherwise
 */
static int free_mem(void *virt_addr)
{
  int r;
  l4re_ds_t ds;

  /* Detach memory from our address space */
  if ((r = l4re_rm_detach_ds(virt_addr, &ds)))
    return r;

  /* Free memory at our memory allocator */
  l4re_util_cap_free_um(ds);

  /* All went ok */
  return 0;
}

int main(void)
{
  void *virt;

  /* Allocate memory: 16k Bytes (usually) */
  if (allocate_mem(4 * L4_PAGESIZE, 0, &virt))
    return 1;

  printf("Allocated memory.\n");

  /* Do something with the memory */
  memset(virt, 0x12, 4 * L4_PAGESIZE);

  printf("Touched memory.\n");

  /* Free memory */
  if (free_mem(virt))
    return 2;

  printf("Freed and done. Bye.\n");

  return 0;
}

/*!
 * \file	iomem.c
 * \brief	Handling of I/O memory mapped memory
 *
 * \date	07/2002
 * \author	Frank Mehnert <fm3@os.inf.tu-dresden.de> */
/*
 * (c) 2002-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <stdio.h>
#include <stdlib.h>

#include <l4/sys/types.h>
#include <l4/util/l4_macros.h>
#include <l4/io/io.h>

#include "iomem.h"

int
map_io_mem(l4_addr_t paddr, l4_size_t size, int cacheable,
	   const char *id, l4_addr_t *vaddr)
{
   if (l4io_request_iomem(paddr, size,
                          cacheable ? L4IO_MEM_WRITE_COMBINED : 0, vaddr))
        { printf("Can't request mem region from l4io.\n"); exit(1); }

   printf("Mapped I/O %s mem  "l4_addr_fmt" => "l4_addr_fmt" [%zdkB] via l4io\n",
               id, paddr, *vaddr, size >> 10);

  return 0;
}

void
unmap_io_mem(l4_addr_t addr, l4_size_t size, const char *id, l4_addr_t vaddr)
{
  int error;

  (void)vaddr;
  if ((error = l4io_release_iomem(addr , size)))
      printf("Error %d releasing region "l4_addr_fmt"-"l4_addr_fmt" at l4io\n",
	    error, addr, addr+size);
  printf("Unmapped I/O %s mem via l4io\n", id);
}

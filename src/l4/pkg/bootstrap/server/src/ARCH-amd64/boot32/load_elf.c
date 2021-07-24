/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2009-2020 Kernkonzept GmbH.
 * Authors: Alexander Warg <warg@os.inf.tu-dresden.de>
 *          Frank Mehnert <fm3@os.inf.tu-dresden.de>
 *          Marcus Haehnel <marcus.haehnel@kernkonzept.com>
 */

#include <stdio.h>
#include <string.h>

#include "types.h"
#include <l4/util/elf.h>
#include "load_elf.h"
#include "support.h"


l4_uint32_t
load_elf (void *elf)
{
  char *_elf = (char *) elf;
  Elf64_Ehdr *eh = (Elf64_Ehdr *)(_elf);
  Elf64_Phdr *ph = (Elf64_Phdr *)(_elf + eh->e_phoff);
  int i;

  for (i = 0; i < eh->e_phnum; i++, ph++)
    {
      if (ph->p_type != PT_LOAD)
        continue;

      if (ph->p_paddr + ph->p_memsz > 1ULL << 32)
        panic("Could not load PHDR. Target exceeds 32-bit limits!");

      if (overlaps_reservation((void*)(Elf32_Addr)ph->p_paddr, ph->p_filesz))
        panic("The link address is already occupied by other data!");

      if ((unsigned long)_elf >= (1ULL << 32) - ph->p_offset)
        panic("Could not load PHDR. Location in ELF exceeds 32-bit limits!");

      memcpy((void*)((Elf32_Addr)ph->p_paddr),
             _elf + ph->p_offset, ph->p_filesz);

      if (ph->p_filesz < ph->p_memsz)
        {
          if (overlaps_reservation((void*)(Elf32_Addr)ph->p_paddr, ph->p_memsz))
            panic("The link address is already occupied by other data!");
          memset((void*)((Elf32_Addr)(ph->p_paddr + ph->p_filesz)), 0,
                 ph->p_memsz - ph->p_filesz);
        }
    }


  return eh->e_entry;
}

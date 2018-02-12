/**
 * \file	bootstrap/server/src/exec.c
 * \brief	ELF loader
 * 
 * \date	2004
 * \author	Frank Mehnert <fm3@os.inf.tu-dresden.de>,
 *		Torsten Frenzel <frenzel@os.inf.tu-dresden.de> */

/*
 * (c) 2005-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <alloca.h>

#include <l4/util/elf.h>

#include "exec.h"

int
exec_load_elf(exec_handler_func_t *handler,
              void *handle, const char **error_msg,
              l4_addr_t *entry)
{
  void **t = handle;
  ElfW(Ehdr) *x = *t;
  ElfW(Phdr) *phdr, *ph;
  int i;

  /* Read the ELF header.  */

  if (!l4util_elf_check_magic(x))
    return *error_msg="no ELF executable", -1;

  /* Make sure the file is of the right architecture.  */
  if (!l4util_elf_check_arch(x))
    return *error_msg="wrong ELF architecture", -1;

  if (entry)
    *entry = (l4_addr_t) x->e_entry;

  phdr   = l4util_elf_phdr(x);

  for (i = 0; i < x->e_phnum; i++)
    {
      ph = (ElfW(Phdr)*)((l4_addr_t)phdr + i * x->e_phentsize);
      if (ph->p_type == 0)
        continue;

      exec_sectype_t type = 0;
      if (ph->p_type == PT_LOAD)
        type |= EXEC_SECTYPE_ALLOC | EXEC_SECTYPE_LOAD;

      if (ph->p_type == 0x10) // KIP PHDR
        type |= EXEC_SECTYPE_KIP;

      if (ph->p_type == 0x11) // KIP PHDR
        type |= EXEC_SECTYPE_KOPT;

      if (ph->p_flags & PF_R)
        type |= EXEC_SECTYPE_READ;

      if (ph->p_flags & PF_W)
        type |= EXEC_SECTYPE_WRITE;

      if (ph->p_flags & PF_X)
        type |= EXEC_SECTYPE_EXECUTE;

      int res = (*handler)(handle,
                           ph->p_offset, ph->p_filesz,
                           ph->p_paddr, ph->p_vaddr, ph->p_memsz, type);
      if (res != 0)
        return *error_msg="", res;
    }

  return *error_msg="", 0;
}

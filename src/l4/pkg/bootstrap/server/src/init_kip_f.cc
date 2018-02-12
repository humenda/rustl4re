/**
 * \file
 */
/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <stdio.h>
#include <string.h>
#include <l4/sys/kip.h>
#include <l4/util/l4_macros.h>
#include "panic.h"

#include "macros.h"
#include "init_kip.h"
#include "region.h"
#include "startup.h"
#include <l4/sys/kip>

using L4::Kip::Mem_desc;

/**
 * setup Kernel Info Page
 */
void
init_kip_f(void *_l4i, boot_info_t *bi, l4util_mb_info_t *mbi,
           Region_list *ram, Region_list *regions)
{
  l4_kernel_info_t *l4i = (l4_kernel_info_t *)_l4i;

  unsigned char l4_api = l4i->version >> 24;

  if (l4_api != 0x87 /*VERSION_FIASCO*/)
    panic("cannot load kernels other than Fiasco");

  Mem_desc *md = Mem_desc::first(_l4i);
  for (Region const* c = ram->begin(); c != ram->end(); ++c)
    (md++)->set(c->begin(), c->end(), Mem_desc::Conventional);

  for (Region const *c = regions->begin(); c != regions->end(); ++c)
    {
      Mem_desc::Mem_type type = Mem_desc::Reserved;
      unsigned char sub_type = 0;
      switch (c->type())
	{
	case Region::No_mem:
	case Region::Ram:
	case Region::Boot:
	  continue;
	case Region::Kernel:
	  type = Mem_desc::Reserved;
	  break;
	case Region::Sigma0:
	  type = Mem_desc::Dedicated;
	  break;
	case Region::Root:
	  type = Mem_desc::Bootloader;
	  break;
	case Region::Arch:
	  type = Mem_desc::Arch;
	  sub_type = c->sub_type();
	  break;
        case Region::Info:
          type = Mem_desc::Info;
          sub_type = c->sub_type();
          break;
        }
      (md++)->set(c->begin(), c->end() - 1, type, sub_type);
    }

  l4i->user_ptr = (unsigned long)mbi;

  /* set up sigma0 info */
  l4i->sigma0_eip = bi->sigma0_start;
  printf("  Sigma0 config    ip:" l4_addr_fmt " sp:" l4_addr_fmt "\n",
         l4i->sigma0_eip, l4i->sigma0_esp);

  /* set up roottask info */
  l4i->root_eip          = bi->roottask_start;
  printf("  Roottask config  ip:" l4_addr_fmt " sp:" l4_addr_fmt "\n",
         l4i->root_eip, l4i->root_esp);

  /* Platform info */
  strncpy(l4i->platform_info.name, PLATFORM_TYPE,
          sizeof(l4i->platform_info.name));
  l4i->platform_info.name[sizeof(l4i->platform_info.name) - 1] = 0;
}

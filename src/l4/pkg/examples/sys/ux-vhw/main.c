/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/sys/ipc.h>
#include <l4/sys/vhw.h>
#include <l4/util/util.h>
#include <l4/util/kip.h>
#include <l4/re/env.h>

#include <stdlib.h>
#include <stdio.h>

static void print_entry(struct l4_vhw_entry *e)
{
  printf("type: %d  mem start: %08lx  end: %08lx\n"
         "irq: %d pid %d\n",
	 e->type, e->mem_start, e->mem_size,
	 e->irq_no, e->provider_pid);
}

int main(void)
{
  l4_kernel_info_t *kip = l4re_kip();
  struct l4_vhw_descriptor *vhw;
  int i;

  if (!kip)
    {
      printf("KIP not available!\n");
      return 1;
    }

  if (!l4util_kip_kernel_is_ux(kip))
    {
      printf("This example is for Fiasco-UX only.\n");
      return 1;
    }

  vhw = l4_vhw_get(kip);

  printf("kip at %p, vhw at %p\n", kip, vhw);
  printf("magic: %08x, version: %08x, count: %02d\n",
         vhw->magic, vhw->version, vhw->count);

  for (i = 0; i < vhw->count; i++)
    print_entry(l4_vhw_get_entry(vhw, i));

  return 0;
}

/*
 * (c) 2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *          Alexander Warg <warg@os.inf.tu-dresden.de>,
 *          Frank Mehnert <fm3@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <stdio.h>

#include <l4/util/mb_info.h>
#include "boot_cpu.h"
#include "boot_paging.h"
#include "load_elf.h"
#include "support.h"

extern void _exit(int rc);

extern unsigned KERNEL_CS_64;
extern char _binary_bootstrap64_bin_start;
extern char _image_start;
extern char _image_end;

l4_uint32_t rsdp_start = 0;
l4_uint32_t rsdp_end = 0;

static l4_uint64_t find_upper_mem(l4util_mb_info_t *mbi)
{
  l4_uint64_t max = 0;
  l4util_mb_addr_range_t *mmap;
  l4util_mb_for_each_mmap_entry(mmap, mbi)
    {
      if (max < mmap->addr + mmap->size)
        max = mmap->addr + mmap->size;
    }
  return max;
}

static void
reserve_mbi(l4util_mb_info_t *mbi)
{
  reservation_add((unsigned long)mbi, sizeof(l4util_mb_info_t));
  reservation_add(mbi->cmdline, __builtin_strlen((char*)mbi->cmdline) + 1);

  l4util_mb_mod_t *mods = (l4util_mb_mod_t*)mbi->mods_addr;
  reservation_add((unsigned long)mods,
                  mbi->mods_count * sizeof(l4util_mb_mod_t));

  for (l4util_mb_mod_t *mod = mods; mod < mods + mbi->mods_count; ++mod)
    {
      reservation_add(mod->cmdline, __builtin_strlen((char *)mod->cmdline) + 1);
      reservation_add(mod->mod_start, mod->mod_end - mod->mod_start);
    }
}

void bootstrap (l4util_mb_info_t *mbi, unsigned int flag, char *rm_pointer);
void
bootstrap (l4util_mb_info_t *mbi, unsigned int flag, char *rm_pointer)
{
  struct
  {
    l4_uint32_t start;
    l4_uint16_t cs;
  } far_ptr;
  l4_uint64_t mem_upper;

  // setup stuff for base_paging_init()
  base_cpu_setup();

  mem_upper = find_upper_mem(mbi);
  if (!mem_upper)
    mem_upper = 1024 * (1024 + mbi->mem_upper);

  printf("Highest physical memory address found: %llx (%llxMiB)\n",
         mem_upper, mem_upper >> 20);
  // our memory available for our initial identity mapped page table is
  // enough to cover 4GB of physical memory that must contain anything that
  // is required to boot, i.e. bootstrap, all modules, any required devices
  // and the final location of sigma0 and moe
  const unsigned long long Max_initial_mem = 4ull << 30;

  // Make sure future allocations do not overwrite our image
  reservation_add((unsigned long)&_image_start,
                  (unsigned long)(&_image_end - &_image_start));
  reservation_add(0, 0x1000);      // null page
  reservation_add(0x1000, 0x1000); // fiasco trampoline page

  reserve_mbi(mbi);

  if (mem_upper > Max_initial_mem)
    mem_upper = Max_initial_mem;

  boot32_info.rsdp_start = rsdp_start;
  boot32_info.rsdp_end = rsdp_end;

  // now do base_paging_init(): sets up paging with one-to-one mapping
  base_paging_init(round_superpage(mem_upper));

  printf("Loading 64bit part...\n");
  // switch from 32 Bit compatibility mode to 64 Bit mode
  far_ptr.cs    = KERNEL_CS_64;
  far_ptr.start = load_elf(&_binary_bootstrap64_bin_start);

  asm volatile("ljmp *(%4)"
                :: "D"(mbi), "S"(flag), "d"(rm_pointer),
                   "c"(&boot32_info),
                   "r"(&far_ptr), "m"(far_ptr));
}

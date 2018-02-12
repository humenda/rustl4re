/*!
 * \file   support_x86.cc
 * \brief  Support for the x86 platform
 *
 * \date   2008-01-02
 * \author Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *
 */
/*
 * (c) 2008-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include "support.h"
#include "x86_pc-base.h"

#include <string.h>
#include "startup.h"
#include "panic.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

static char rsdp_tmp_buf[36];
l4_uint32_t rsdp_start;
l4_uint32_t rsdp_end;

enum { Verbose_mbi = 1 };

namespace {

struct Platform_x86_1 : Platform_x86
{
  l4util_mb_info_t *mbi;

  void setup_memory_map()
  {
    Region_list *ram = mem_manager->ram;
    Region_list *regions = mem_manager->regions;

#ifdef ARCH_amd64
    // add the page-table on which we're running in 64bit mode
    regions->add(Region::n(boot32_info->ptab64_addr,
                           boot32_info->ptab64_addr + boot32_info->ptab64_size,
                           ".bootstrap-ptab64", Region::Boot));
#endif

#ifdef ARCH_amd64
    rsdp_start = boot32_info->rsdp_start;
    rsdp_end = boot32_info->rsdp_end;
#endif

   // if we were passed the RSDP structure, save it aside
   if ((rsdp_end - rsdp_start) <= sizeof(rsdp_tmp_buf))
     memcpy(rsdp_tmp_buf, (void *)(l4_addr_t)rsdp_start, rsdp_end - rsdp_start);
   else
     {
       rsdp_start = 0;
       rsdp_end = 0;
     }

   if (!(mbi->flags & L4UTIL_MB_MEM_MAP))
      {
        assert(mbi->flags & L4UTIL_MB_MEMORY);
        ram->add(Region::n(0, (mbi->mem_lower + 1024) << 10, ".ram",
                           Region::Ram));
        ram->add(Region::n(0x100000, (mbi->mem_upper + 1024) << 10, ".ram",
                           Region::Ram));

        // Fix EBDA in conventional memory
        unsigned long p = *(l4_uint16_t *)0x40e << 4;

        if (p > 0x400)
          {
            unsigned long e = p + 1024;
            Region *r = ram->find(Region(p, e - 1));
            if (r)
              {
                if (e - 1 < r->end())
                  ram->add(Region::n(e, r->end(), ".ram", Region::Ram), true);
                r->end(p);
              }
          }
      }
    else
      {
        l4util_mb_addr_range_t *mmap;
        l4util_mb_for_each_mmap_entry(mmap, mbi)
          {
            unsigned long long start = (unsigned long long)mmap->addr;
            unsigned long long end = (unsigned long long)mmap->addr + mmap->size;

            switch (mmap->type)
              {
              case 1:
                ram->add(Region::n(start, end, ".ram", Region::Ram));
                break;
              case 2:
              case 3:
              case 4:
                regions->add(Region::n(start, end, ".BIOS", Region::Arch, mmap->type));
                break;
              case 5:
                regions->add(Region::n(start, end, ".BIOS", Region::No_mem));
                break;
              case 20:
                regions->add(Region::n(start, end, ".BIOS", Region::Arch, mmap->type));
                break;
              default:
                break;
              }
          }
      }

    regions->add(Region::n(0, 0x1000, ".BIOS", Region::Arch, 0));
  }
};

#ifdef IMAGE_MODE

class Platform_x86_loader_mbi :
  public Platform_x86_1,
  public Boot_modules_image_mode
{
public:
  Boot_modules *modules() { return this; }

};

Platform_x86_loader_mbi _x86_pc_platform;

#else // IMAGE_MODE

class Platform_x86_multiboot : public Platform_x86_1, public Boot_modules
{
public:
  Boot_modules *modules() { return this; }

  Module module(unsigned index, bool) const
  {
    Module m;
    l4util_mb_mod_t *mb_mod = (l4util_mb_mod_t*)(unsigned long)mbi->mods_addr;

    m.start   = (char const *)(l4_addr_t)mb_mod[index].mod_start;
    m.end     = (char const *)(l4_addr_t)mb_mod[index].mod_end;
    m.cmdline = (char const *)(l4_addr_t)mb_mod[index].cmdline;
    return m;
  }

  unsigned num_modules() const { return mbi->mods_count; }

  void reserve()
  {
    Region_list *regions = mem_manager->regions;

    regions->add(Region::n((unsigned long)mbi,
                           (unsigned long)mbi + sizeof(*mbi),
                           ".mbi", Region::Boot));

    if (mbi->flags & L4UTIL_MB_CMDLINE)
      regions->add(Region::n((unsigned long)mbi->cmdline,
                             (unsigned long)mbi->cmdline
                             + strlen((char const *)(l4_addr_t)mbi->cmdline) + 1,
                             ".mbi", Region::Boot));

    l4util_mb_mod_t *mb_mod = (l4util_mb_mod_t*)(unsigned long)mbi->mods_addr;
    regions->add(Region::n((unsigned long)mb_mod,
                           (unsigned long)&mb_mod[mbi->mods_count],
                           ".mbi", Region::Boot));

    if (mbi->flags & L4UTIL_MB_VIDEO_INFO)
      {
        if (mbi->vbe_mode_info)
          regions->add(Region::start_size(mbi->vbe_mode_info,
                                          sizeof(l4util_mb_vbe_mode_t),
                                          ".mbi", Region::Boot));
        if (mbi->vbe_ctrl_info)
          regions->add(Region::start_size(mbi->vbe_ctrl_info,
                                          sizeof(l4util_mb_vbe_ctrl_t),
                                          ".mbi", Region::Boot));
      }


    for (unsigned i = 0; i < mbi->mods_count; ++i)
      regions->add(Region::n(mb_mod[i].cmdline,
                             (unsigned long)mb_mod[i].cmdline
                             + strlen((char const *)(l4_addr_t)mb_mod[i].cmdline) + 1,
                             ".mbi", Region::Boot));

    for (unsigned i = 0; i < mbi->mods_count; ++i)
      {
        /*
         * Avoid overflow on size calculation of empty modules,
         * i.e. mod_start == mod_end. Grub generates MBI entries
         * with start == end == 0 for empty files loaded as modules.
         */
        if (mb_mod[i].mod_start >= mb_mod[i].mod_end)
          panic("Found a module with unplausible size (%s). Abort.\n",
                (char const*)(l4_addr_t)mb_mod[i].cmdline);
        regions->add(mod_region(i, mb_mod[i].mod_start,
                                mb_mod[i].mod_end - mb_mod[i].mod_start));
      }
  }

  void move_module(unsigned index, void *dest,
                   bool overlap_check)
  {
    l4util_mb_mod_t *mod = (l4util_mb_mod_t*)(unsigned long)mbi->mods_addr + index;
    unsigned long size = mod->mod_end - mod->mod_start;
    _move_module(index, dest, (char const *)(l4_addr_t)mod->mod_start,
                 size, overlap_check);

    assert ((l4_addr_t)dest < 0xfffffff0);
    assert ((l4_addr_t)dest < 0xfffffff0 - size);
    mod->mod_start = (l4_addr_t)dest;
    mod->mod_end   = (l4_addr_t)dest + size;
  }

  l4util_mb_info_t *construct_mbi(unsigned long mod_addr)
  {
    // calculate the size needed to cover the full MBI, including command lines
    unsigned long total_size = sizeof(l4util_mb_info_t);

    // consider the global command line
    if (mbi->flags & L4UTIL_MB_CMDLINE)
      total_size += round_wordsize(strlen((char const *)(l4_addr_t)mbi->cmdline) + 1);

    // consider VBE info
    if (mbi->flags & L4UTIL_MB_VIDEO_INFO)
      {
        if (mbi->vbe_mode_info)
          total_size += sizeof(l4util_mb_vbe_mode_t);

        if (mbi->vbe_ctrl_info)
          total_size += sizeof(l4util_mb_vbe_ctrl_t);
      }

    // consider modules
    total_size += sizeof(l4util_mb_mod_t) * mbi->mods_count;

    // scan through all modules and add the command line
    l4util_mb_mod_t *mods = (l4util_mb_mod_t*)(unsigned long)mbi->mods_addr;
    for (l4util_mb_mod_t *m = mods; m != mods + mbi->mods_count; ++m)
      if (m->cmdline)
        total_size += round_wordsize(strlen((char const *)(l4_addr_t)m->cmdline) + 1);

    if (Verbose_mbi)
      printf("  need %ld bytes to copy MBI\n", total_size);

    // try to find a free region for the MBI
    char *_mb = (char *)mem_manager->find_free_ram(total_size);
    if (!_mb)
      panic("fatal: could not allocate memory for multi-boot info\n");

    // mark the region as reserved
    mem_manager->regions->add(Region::start_size((l4_addr_t)_mb, total_size, ".mbi_rt",
                                                 Region::Root));
    if (Verbose_mbi)
      printf("  reserved %ld bytes at %p\n", total_size, _mb);

    // copy the whole MBI
    l4util_mb_info_t *dst = (l4util_mb_info_t *)_mb;
    *dst = *mbi;

    l4util_mb_mod_t *dst_mods = (l4util_mb_mod_t *)(dst + 1);
    dst->mods_addr = (l4_addr_t)dst_mods;
    _mb = (char *)(dst_mods + mbi->mods_count);

    if (mbi->flags & L4UTIL_MB_VIDEO_INFO)
      {
        if (mbi->vbe_mode_info)
          {
            l4util_mb_vbe_mode_t *d = (l4util_mb_vbe_mode_t *)_mb;
            *d = *((l4util_mb_vbe_mode_t *)(l4_addr_t)mbi->vbe_mode_info);
            dst->vbe_mode_info = (l4_addr_t)d;
            _mb = (char *)(d + 1);
          }

        if (mbi->vbe_ctrl_info)
          {
            l4util_mb_vbe_ctrl_t *d = (l4util_mb_vbe_ctrl_t *)_mb;
            *d = *((l4util_mb_vbe_ctrl_t *)(l4_addr_t)mbi->vbe_ctrl_info);
            dst->vbe_ctrl_info = (l4_addr_t)d;
            _mb = (char *)(d + 1);
          }
      }

    if (mbi->cmdline)
      {
        strcpy(_mb, (char const *)(l4_addr_t)mbi->cmdline);
        dst->cmdline = (l4_addr_t)_mb;
        _mb += round_wordsize(strlen(_mb) + 1);
      }

    for (unsigned i = 0; i < mbi->mods_count; ++i)
      {
        dst_mods[i] = mods[i];
        if (char const *c = (char const *)(l4_addr_t)(mods[i].cmdline))
          {
            unsigned l = strlen(c) + 1;
            dst_mods[i].cmdline = (l4_addr_t)_mb;
            memcpy(_mb, c, l);
            _mb += round_wordsize(l);
          }
      }

    mbi = dst;

    // remove the old MBI from the reserved memory
    for (Region *r = mem_manager->regions->begin();
         r != mem_manager->regions->end();)
      {
        if (strcmp(r->name(), ".mbi"))
          ++r;
        else
          r = mem_manager->regions->remove(r);
      }

    move_modules(mod_addr);

    // Our aim here is to allocate an aligned chunk of memory for our copy of
    // RSDP and create a region out of it.
    //
    // XXX: For now, we are piggybacking on this callback because there is
    // currently no better one that is called this late.
    if (rsdp_start)
      {
        enum {
          // XXX: need a single definition of this
          Info_acpi_rsdp = 0
        };
        char *rsdp_buf =
          (char *)mem_manager->find_free_ram(sizeof(rsdp_tmp_buf));
        if (!rsdp_buf)
          panic("fatal: could not allocate memory for RSDP\n");
        memcpy(rsdp_buf, rsdp_tmp_buf, sizeof(rsdp_tmp_buf));

        mem_manager->regions->add(
          Region::n((l4_addr_t)rsdp_buf,
                    (l4_addr_t)rsdp_buf + sizeof(rsdp_tmp_buf), ".ACPI",
                    Region::Info, Info_acpi_rsdp));
      }

    return mbi;
  }
};

Platform_x86_multiboot _x86_pc_platform;

#endif // !IMAGE_MODE
}

namespace /* usb_xhci_handoff */ {

static int
xhci_first_cap(L4::Io_register_block const *base)
{
  return ((base->read<l4_uint32_t>(0x10) >> 16) & 0xffff) << 2;
}

static int
xhci_next_cap(L4::Io_register_block const *base, int cap)
{
  l4_uint32_t next = (base->read<l4_uint32_t>(cap) >> 8) & 0xff;

  if (!next)
    return 0;

  return cap + (next << 2);
}

static void
udelay(int usecs)
{
  // Assume that we have a 2GHz machine and it needs approximately
  // two cycles per inner loop, this delay function should wait the
  // given amount of micro seconds.
  for (; usecs >= 0; --usecs)
    for (unsigned long i = 1000; i > 0; --i)
      asm volatile ("nop; pause" : : : "memory");
}

static void
do_handoff_cap(L4::Io_register_block const *base, unsigned cap)
{
  enum : l4_uint32_t
  {
    HC_BIOS_OWNED_SEM = 1U << 16,
    HC_OS_OWNED_SEM   = 1U << 24,
    USBLEGCTLSTS      = 0x04,
    DISABLE_SMI       = 0xfff1e011U,
    SMI_EVENTS        = 0x7U << 29,
  };

  l4_uint32_t val = base->read<l4_uint32_t>(cap);

  if (val & HC_BIOS_OWNED_SEM) // BIOS has the HC
    {
      base->write(cap, val | HC_OS_OWNED_SEM); // We want it

      int to = 300; // wait some seconds for BIOS to release
      while (--to > 0 && (base->read<l4_uint32_t>(cap) & HC_BIOS_OWNED_SEM))
        udelay(10);

      if (to == 0)
        {
          printf("Forcing ownership of XHCI controller from BIOS\n");
          base->write(cap, val & ~HC_BIOS_OWNED_SEM);
        }
    }

  // Disable any BIOS SMIs and clear all SMI events
  base->modify<l4_uint32_t>(cap + USBLEGCTLSTS, DISABLE_SMI, SMI_EVENTS);
}

static void
xhci_handoff(Pci_iterator const &dev)
{
  unsigned v = dev.pci_read(0x04, 32);

  // no mmio enabled -> skip
  if (!(v & 2))
    return;

  if (0)
    // no bus-master -> skip
    if (!(v & 4))
      return;

  // read BAR[0]
  v = dev.pci_read(0x10, 32);

  // invalid -> skip
  if (!v)
    return;

  // BAR[0] is an IO BAR, not XHCI compliant -> skip
  if (v & 1)
    return;

  L4::Io_register_block_mmio base(v & ~0xfUL);

  // Find the Legacy Support Capability register
  for (int cap = xhci_first_cap(&base); cap != 0;
       cap = xhci_next_cap(&base, cap))
    {
      l4_uint32_t v = base.read<l4_uint32_t>(cap);
      if ((v & 0xff) == 1)
        {
          do_handoff_cap(&base, cap);
          break;
        }
    }
}

}

enum
{
  PCI_CLASS_SERIAL_USB_XHCI = 0x0c0330,
};

static void
pci_quirks()
{
  for (Pci_iterator i; i != Pci_iterator::end(); ++i)
    {
      unsigned cc = i.pci_class();
      if (cc == PCI_CLASS_SERIAL_USB_XHCI)
        xhci_handoff(i);
    }
}

extern "C"
void __main(l4util_mb_info_t *mbi, unsigned long p2, char const *realmode_si,
            boot32_info_t const *boot32_info);

void __main(l4util_mb_info_t *mbi, unsigned long p2, char const *realmode_si,
            boot32_info_t const *boot32_info)
{
  ctor_init();
  Platform_base::platform = &_x86_pc_platform;
  _x86_pc_platform.init();
#ifdef ARCH_amd64
  // remember this info to reserve the memory in setup_memory_map later
  _x86_pc_platform.boot32_info = boot32_info;
#else
  (void)boot32_info;
#endif
  char const *cmdline;
  (void)realmode_si;
  assert(p2 == L4UTIL_MB_VALID); /* we need to be multiboot-booted */
  _x86_pc_platform.mbi = mbi;
  cmdline = (char const *)(l4_addr_t)mbi->cmdline;
  _x86_pc_platform.setup_uart(cmdline);
  pci_quirks();

  startup(cmdline);
}



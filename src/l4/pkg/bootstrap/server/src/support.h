/*!
 * \file   support.h
 * \brief  Support header file
 *
 * \date   2008-01-02
 * \author Adam Lackorznynski <adam@os.inf.tu-dresden.de>
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
#ifndef __BOOTSTRAP__SUPPORT_H__
#define __BOOTSTRAP__SUPPORT_H__

#include <l4/drivers/uart_base.h>
#include <l4/util/l4mod.h>
#include "mod_info.h"
#include "region.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

struct boot_args
{
  unsigned long r[4];
};

extern struct boot_args boot_args;

L4::Uart *uart();
void set_stdio_uart(L4::Uart *uart);
void ctor_init();

enum { Verbose_load = 0 };

extern Mod_header *mod_header;
extern Mod_info *module_infos;
void init_modules_infos();

template<typename T>
inline T *l4_round_page(T *p) { return (T*)l4_round_page((l4_addr_t)p); }

template<typename T>
inline T *l4_trunc_page(T *p) { return (T*)l4_trunc_page((l4_addr_t)p); }

static inline void __attribute__((always_inline))
clear_bss()
{
  extern char _bss_start[], _bss_end[];
  extern char crt0_stack_low[], crt0_stack_high[];
  memset(_bss_start, 0, crt0_stack_low - _bss_start);
  memset(crt0_stack_high, 0, _bss_end - crt0_stack_high);
}

static inline unsigned long
round_wordsize(unsigned long s)
{ return (s + sizeof(unsigned long) - 1) & ~(sizeof(unsigned long) - 1); }

struct Memory
{
  Region_list *ram;
  Region_list *regions;
  unsigned long find_free_ram(unsigned long size, unsigned long min_addr = 0,
                              unsigned long max_addr = ~0UL);
  unsigned long find_free_ram_rev(unsigned long size, unsigned long min_addr = 0,
                                  unsigned long max_addr = ~0UL);
};

/**
 * Interface to boot modules.
 *
 * Boot modules can for example be loaded by GRUB, or may be linked
 * into bootstrap.
 */
class Boot_modules
{
public:
  enum { Num_base_modules = 3 };

  /// Main information for each module.
  struct Module
  {
    char const *start;
    char const *end;
    char const *cmdline;

    unsigned long size() const { return end -start; }
  };

  static char const *const Mod_reg;

  virtual ~Boot_modules() = 0;
  virtual void reserve() = 0;
  virtual Module module(unsigned index, bool uncompress = true) const = 0;
  virtual unsigned num_modules() const = 0;
  virtual l4util_l4mod_info *construct_mbi(unsigned long mod_addr) = 0;
  virtual void move_module(unsigned index, void *dest) = 0;
  virtual unsigned base_mod_idx(Mod_info_flags mod_info_mod_type) = 0;
  void move_modules(unsigned long modaddr);
  Region mod_region(unsigned index, l4_addr_t start, l4_addr_t size,
                    Region::Type type = Region::Boot);
  void merge_mod_regions();
  static bool is_base_module(const Mod_info *mod)
  {
    unsigned v = mod->flags & Mod_info_flag_mod_mask;
    return v > 0 && v <= Num_base_modules;
  };

  Module mod_kern() { return module(base_mod_idx(Mod_info_flag_mod_kernel)); }
  Module mod_sigma0() { return module(base_mod_idx(Mod_info_flag_mod_sigma0)); }
  Module mod_roottask() { return module(base_mod_idx(Mod_info_flag_mod_roottask)); }

protected:
  void _move_module(unsigned index, void *dest, void const *src,
                    unsigned long size);
};

inline Boot_modules::~Boot_modules() {}

class Platform_base
{
public:
  virtual ~Platform_base() = 0;
  virtual void init() = 0;
  virtual void setup_memory_map() = 0;
  virtual Boot_modules *modules() = 0;
  virtual bool probe() = 0;

  virtual l4_uint64_t to_phys(l4_addr_t bootstrap_addr)
  { return bootstrap_addr; }

  virtual l4_addr_t to_virt(l4_uint64_t phys_addr)
  { return phys_addr; }

  virtual void reboot()
  {
    while (1)
      ;
  }

#if defined(ARCH_arm) || defined(ARCH_arm64)
  void reboot_psci()
  {
    register unsigned long r0 asm("r0") = 0x84000009;
    asm volatile(
#ifdef ARCH_arm
                 ".arch_extension sec\n"
#endif
                 "smc #0" : : "r" (r0));
  }
#endif

  virtual bool arm_switch_to_hyp() { return false; }

  virtual void boot_kernel(unsigned long entry)
  {
    typedef void (*func)(void);
    ((func)entry)();
    exit(-100);
  }

  // remember the chosen platform
  static Platform_base *platform;

  // find a platform
  static void iterate_platforms()
  {
    extern Platform_base *__PLATFORMS_BEGIN[];
    extern Platform_base *__PLATFORMS_END[];
    for (Platform_base **p = __PLATFORMS_BEGIN; p < __PLATFORMS_END; ++p)
      if (*p && (*p)->probe())
        {
          platform = *p;
          platform->init();
          break;
        }
  }
};

inline Platform_base::~Platform_base() {}

#define REGISTER_PLATFORM(type) \
      static type type##_inst; \
      static type * const __attribute__((section(".platformdata"),used)) type##_inst_p = &type##_inst


/**
 * For image mode we have this utility that implements
 * handling of linked in modules.
 */
class Boot_modules_image_mode : public Boot_modules
{
public:
  void reserve();
  Module module(unsigned index, bool uncompress) const;
  unsigned num_modules() const;
  void move_module(unsigned index, void *dest);
  l4util_l4mod_info *construct_mbi(unsigned long mod_addr);
  unsigned base_mod_idx(Mod_info_flags mod_info_module_flag);

private:
  void decompress_mods(unsigned mod_count,
                       l4_addr_t total_size, l4_addr_t mod_addr);
};


class Platform_single_region_ram : public Platform_base,
  public Boot_modules_image_mode
{
public:
  Boot_modules *modules() { return this; }
  void setup_memory_map();
  virtual void post_memory_hook() {}
};

extern Memory *mem_manager;

#endif /* __BOOTSTRAP__SUPPORT_H__ */

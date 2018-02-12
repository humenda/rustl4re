/**
 * \file	bootstrap/server/src/startup.cc
 * \brief	Main functions
 *
 * \date	09/2004
 * \author	Torsten Frenzel <frenzel@os.inf.tu-dresden.de>,
 *		Frank Mehnert <fm3@os.inf.tu-dresden.de>,
 *		Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *		Alexander Warg <aw11@os.inf.tu-dresden.de>
 *		Sebastian Sumpf <sumpf@os.inf.tu-dresden.de>
 */

/*
 * (c) 2005-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

/* LibC stuff */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

/* L4 stuff */
#include <l4/sys/compiler.h>
#include <l4/util/mb_info.h>
#include <l4/util/l4_macros.h>
#include <l4/util/kip.h>
#include "panic.h"

/* local stuff */
#include "exec.h"
#include "macros.h"
#include "region.h"
#include "module.h"
#include "startup.h"
#include "support.h"
#include "init_kip.h"
#include "patch.h"
#include "koptions.h"

#undef getchar

/* management of allocated memory regions */
static Region_list regions;
static Region __regs[300];

/* management of conventional memory regions */
static Region_list ram;
static Region __ram[16];

static Memory _mem_manager = { &ram, &regions };
Memory *mem_manager = &_mem_manager;

L4_kernel_options::Uart kuart;
unsigned int kuart_flags;

/*
 * IMAGE_MODE means that all boot modules are linked together to one
 * big binary.
 */
#ifdef IMAGE_MODE
static l4_addr_t _mod_addr = RAM_BASE + MODADDR;
#else
static l4_addr_t _mod_addr;
#endif

static const char *builtin_cmdline = CMDLINE;


enum {
  kernel_module,
  sigma0_module,
  roottask_module,
};

/// Info passed through our ELF interpreter code
struct Elf_info
{
  Boot_modules::Module mod;
  Region::Type type;
};

struct Hdr_info
{
  Boot_modules::Module mod;
  unsigned hdr_type;
  l4_addr_t start;
  l4_size_t size;
};

static exec_handler_func_t l4_exec_read_exec;
static exec_handler_func_t l4_exec_add_region;
static exec_handler_func_t l4_exec_find_hdr;

// this function can be provided per architecture
void __attribute__((weak)) print_cpu_info();

#if 0
static void
dump_mbi(l4util_mb_info_t *mbi)
{
  printf("%p-%p\n", (void*)(mbi->mem_lower << 10), (void*)(mbi->mem_upper << 10));
  printf("MBI:     [%p-%p]\n", mbi, mbi + 1);
  printf("MODINFO: [%p-%p]\n", (char*)mbi->mods_addr,
      (l4util_mb_mod_t*)(mbi->mods_addr) + mbi->mods_count);

  printf("VBEINFO: [%p-%p]\n", (char*)mbi->vbe_ctrl_info,
      (l4util_mb_vbe_ctrl_t*)(mbi->vbe_ctrl_info) + 1);
  printf("VBEMODE: [%p-%p]\n", (char*)mbi->vbe_mode_info,
      (l4util_mb_vbe_mode_t*)(mbi->vbe_mode_info) + 1);

  l4util_mb_mod_t *m = (l4util_mb_mod_t*)(mbi->mods_addr);
  l4util_mb_mod_t *me = m + mbi->mods_count;
  for (; m < me; ++m)
    {
      printf("  MOD: [%p-%p]\n", (void*)m->mod_start, (void*)m->mod_end);
      printf("  CMD: [%p-%p]\n", (char*)m->cmdline,
	  (char*)m->cmdline + strlen((char*)m->cmdline));
    }
}
#endif


/**
 * Scan the memory regions with type == Region::Kernel for a
 * kernel interface page (KIP).
 *
 * After loading the kernel we scan for the magic number at page boundaries.
 */
static
void *find_kip(Boot_modules::Module const &mod)
{
  unsigned char *p, *end;
  void *k = 0;

  printf("  find kernel info page...\n");

  const char *error_msg;
  Hdr_info hdr;
  hdr.mod = mod;
  hdr.hdr_type = EXEC_SECTYPE_KIP;
  int r = exec_load_elf(l4_exec_find_hdr, &hdr.mod,
                        &error_msg, NULL);

  if (r == 1)
    {
      printf("  found kernel info page (via ELF) at %lx\n", hdr.start);
      return (void *)hdr.start;
    }

  for (Region const *m = regions.begin(); m != regions.end(); ++m)
    {
      if (m->type() != Region::Kernel)
	continue;

      if (sizeof(unsigned long) < 8
          && m->end() >= (1ULL << 32))
	end = (unsigned char *)(~0UL - 0x1000);
      else
	end = (unsigned char *) (unsigned long)m->end();

      for (p = (unsigned char *) (unsigned long)(m->begin() & 0xfffff000);
	   p < end;
	   p += 0x1000)
	{
	  l4_umword_t magic = L4_KERNEL_INFO_MAGIC;
	  if (memcmp(p, &magic, 4) == 0)
	    {
	      k = p;
	      printf("  found kernel info page at %p\n", p);
	      break;
	    }
	}

      if (k)
        break;
    }

  if (!k)
    panic("could not find kernel info page, maybe your kernel is too old");

  return k;
}

static
L4_kernel_options::Options *find_kopts(Boot_modules::Module const &mod, void *kip)
{
  const char *error_msg;
  Hdr_info hdr;
  hdr.mod = mod;
  hdr.hdr_type = EXEC_SECTYPE_KOPT;
  int r = exec_load_elf(l4_exec_find_hdr, &hdr.mod,
                        &error_msg, NULL);

  if (r == 1)
    {
      printf("  found kernel options (via ELF) at %lx\n", hdr.start);
      return (L4_kernel_options::Options *)hdr.start;
    }

  unsigned long a = (unsigned long)kip + sizeof(l4_kernel_info_t);

  // kernel-option directly follow the KIP page
  a = (a + L4_PAGESIZE - 1) & L4_PAGEMASK;

  L4_kernel_options::Options *ko = (L4_kernel_options::Options *)a;

  if (ko->magic != L4_kernel_options::Magic)
    panic("Could not find kernel options page");

  unsigned long need_version = 1;

  if (ko->version != need_version)
    panic("Cannot boot kernel with incompatible options version: %lu, need %lu",
          (unsigned long)ko->version, need_version);

  return ko;
}

/**
 * Get the API version from the KIP.
 */
static inline
unsigned long get_api_version(void *kip)
{
  return ((unsigned long *)kip)[1];
}


static char const *
check_arg_str(char const *cmdline, const char *arg)
{
  char const *s = cmdline;
  while ((s = strstr(s, arg)))
    {
      if (s == cmdline
          || isspace(s[-1]))
        return s;
    }
  return NULL;
}

static char *
check_arg_str(char *cmdline, const char *arg)
{
  return const_cast<char *>(check_arg_str(const_cast<char const *>(cmdline), arg));
}

/**
 * Scan the command line for the given argument.
 *
 * The cmdline string may either be including the calling program
 * (.../bootstrap -arg1 -arg2) or without (-arg1 -arg2) in the realmode
 * case, there, we do not have a leading space
 *
 * return pointer after argument, NULL if not found
 */
char const *
check_arg(char const *cmdline, const char *arg)
{
  if (cmdline)
    return check_arg_str(cmdline, arg);

  return NULL;
}


/**
 * Calculate the maximum memory limit in MB.
 *
 * The limit is the highest physical address where conventional RAM is allowed.
 * The memory is limited to 3 GB IA32 and unlimited on other systems.
 */
static
unsigned long long
get_memory_max_address()
{
#ifndef __LP64__
  /* Limit memory, we cannot really handle more right now. In fact, the
   * problem is roottask. It maps as many superpages/pages as it gets.
   * After that, the remaining pages are mapped using l4sigma0_map_anypage()
   * with a receive window of L4_WHOLE_ADDRESS_SPACE. In response Sigma0
   * could deliver pages beyond the 3GB user space limit. */
  return 3024ULL << 20;
#endif

  return ~0ULL;
}

/*
 * If available the '-maxmem=xx' command line option is used.
 */
static
unsigned long long
get_memory_max_size(char const *cmdline)
{
  /* maxmem= parameter? */
  if (char const *c = check_arg(cmdline, "-maxmem="))
    return strtoul(c + 8, NULL, 10) << 20;

  return ~0ULL;
}

static int
parse_memvalue(const char *s, unsigned long *val, char **ep)
{

  *val = strtoul(s, ep, 0);
  if (*val == ~0UL)
    return 1;

  switch (**ep)
    {
    case 'G': *val <<= 10; /* FALLTHRU */
    case 'M': *val <<= 10; /* FALLTHRU */
    case 'k': case 'K': *val <<= 10; (*ep)++;
    };

  return 0;
}

/*
 * Parse a memory layout string: size@offset
 * E.g.: 256M@0x40000000, or 128M@128M
 */
static int
parse_mem_layout(const char *s, unsigned long *sz, unsigned long *offset)
{
  char *ep;

  if (parse_memvalue(s, sz, &ep))
    return 1;

  if (*sz == 0)
    return 1;

  if (*ep != '@')
    return 1;

  if (parse_memvalue(ep + 1, offset, &ep))
    return 1;

  return 0;
}

static void
dump_ram_map(bool show_total = false)
{
  // print RAM summary
  unsigned long long sum = 0;
  for (Region *i = ram.begin(); i < ram.end(); ++i)
    {
      printf("  RAM: %016llx - %016llx: %lldkB\n",
             i->begin(), i->end(), i->size() >> 10);
      sum += i->size();
    }
  if (show_total)
    printf("  Total RAM: %lldMB\n", sum >> 20);
}

static void
setup_memory_map(char const *cmdline)
{
  bool parsed_mem_option = false;
  const char *s = cmdline;

  if (s)
    {
      while ((s = check_arg_str((char *)s, "-mem=")))
        {
          s += 5;
          unsigned long sz, offset = 0;
          if (!parse_mem_layout(s, &sz, &offset))
            {
              parsed_mem_option = true;
              ram.add(Region::n(offset, offset + sz, ".ram", Region::Ram));
            }
        }
    }

  if (!parsed_mem_option)
    // No -mem option given, use the one given by the platform
    Platform_base::platform->setup_memory_map();

  dump_ram_map(true);
}

static void do_the_memset(unsigned long s, unsigned val, unsigned long len)
{
  printf("Presetting memory %16lx - %16lx to '%x'\n",
         s, s + len - 1, val);
  memset((void *)s, val, len);
}

static void fill_mem(unsigned fill_value)
{
  regions.sort();
  for (Region const *r = ram.begin(); r != ram.end(); ++r)
    {
      unsigned long long b = r->begin();
      // The regions list must be sorted!
      for (Region const *reg = regions.begin(); reg != regions.end(); ++reg)
        {
          // completely before ram?
          if (reg->end() < r->begin())
            continue;
          // completely after ram?
          if (reg->begin() > r->end())
            break;

          if (reg->begin() <= r->begin())
            b = reg->end() + 1;
          else if (b > reg->begin()) // some overlapping
            {
              if (reg->end() + 1 > b)
                b = reg->end() + 1;
            }
          else
            {
              do_the_memset(b, fill_value, reg->begin() - 1 - b + 1);
              b = reg->end() + 1;
            }
        }

      if (b < r->end())
        do_the_memset(b, fill_value, r->end() - b + 1);
    }
}


/**
 * Add the bootstrap binary itself to the allocated memory regions.
 */
static void
init_regions()
{
  extern int _start;	/* begin of image -- defined in crt0.S */
  extern int _end;	/* end   of image -- defined by bootstrap.ld */

  auto *p = Platform_base::platform;

  regions.add(Region::n(p->to_phys((unsigned long)&_start),
                        p->to_phys((unsigned long)&_end),
                        ".bootstrap", Region::Boot));
}

/**
 * Add all sections of the given ELF binary to the allocated regions.
 * Actually does not load the ELF binary (see load_elf_module()).
 */
static void
add_elf_regions(Boot_modules::Module const &m, Region::Type type)
{
  Elf_info info;
  int r;
  const char *error_msg;

  info.type = type;
  info.mod = m;

  printf("  Scanning %s\n", m.cmdline);

  r = exec_load_elf(l4_exec_add_region, &info,
                    &error_msg, NULL);

  if (r)
    {
      if (Verbose_load)
        {
          printf("\n%p: ", (void*)m.start);
          for (int i = 0; i < 4; ++i)
            printf("%08x ", *((unsigned *)m.start + i));
          printf("  ");
          for (int i = 0; i < 16; ++i)
            {
              unsigned char c = *(unsigned char *)((char *)m.start + i);
              printf("%c", c < 32 ? '.' : c);
            }
        }
      panic("\n\nThis is an invalid binary, fix it (%s).", error_msg);
    }
}


/**
 * Load the given ELF binary into memory and free the source
 * memory region.
 */
static l4_addr_t
load_elf_module(Boot_modules::Module const &mod)
{
  l4_addr_t entry;
  int r;
  const char *error_msg;

  r = exec_load_elf(l4_exec_read_exec, const_cast<Boot_modules::Module *>(&mod),
                    &error_msg, &entry);

  if (r)
    printf("  => can't load module (%s)\n", error_msg);
  else
    {
      Region m = Region::n(mod.start, l4_round_page(mod.end));
      if (!regions.sub(m))
        {
          Region m = Region::n(mod.start, mod.end);
          if (!regions.sub(m))
            {
              m.vprint();
              regions.dump();
              panic ("could not free region for load ELF module");
            }
        }
    }

  return entry;
}

/**
 * Simple linear memory allocator.
 *
 * Allocate size bytes starting from *ptr and set *ptr to *ptr + size.
 */
static inline void*
lin_alloc(l4_size_t size, char **ptr)
{
  void *ret = *ptr;
  *ptr += (size + 3) & ~3;;
  return ret;
}


#ifdef ARCH_arm
static inline l4_umword_t
running_in_hyp_mode()
{
  l4_umword_t cpsr;
  asm volatile("mrs %0, cpsr" : "=r"(cpsr));
  return (cpsr & 0x1f) == 0x1a;
}

static void
setup_and_check_kernel_config(Platform_base *plat, l4_kernel_info_t *kip)
{
  l4_kip_platform_info_arch *ia = &kip->platform_info.arch;

  asm("mrc p15, 0, %0, c0, c0, 0" : "=r" (ia->cpuinfo.MIDR));
  asm("mrc p15, 0, %0, c0, c0, 1" : "=r" (ia->cpuinfo.CTR));
  asm("mrc p15, 0, %0, c0, c0, 2" : "=r" (ia->cpuinfo.TCMTR));
  asm("mrc p15, 0, %0, c0, c0, 3" : "=r" (ia->cpuinfo.TLBTR));
  asm("mrc p15, 0, %0, c0, c0, 5" : "=r" (ia->cpuinfo.MPIDR));
  asm("mrc p15, 0, %0, c0, c0, 6" : "=r" (ia->cpuinfo.REVIDR));

  if (((ia->cpuinfo.MIDR >> 16) & 0xf) >= 7)
    {
      asm("mrc p15, 0, %0, c0, c1, 0" : "=r" (ia->cpuinfo.ID_PFR[0]));
      asm("mrc p15, 0, %0, c0, c1, 1" : "=r" (ia->cpuinfo.ID_PFR[1]));
      asm("mrc p15, 0, %0, c0, c1, 2" : "=r" (ia->cpuinfo.ID_DFR0));
      asm("mrc p15, 0, %0, c0, c1, 3" : "=r" (ia->cpuinfo.ID_AFR0));
      asm("mrc p15, 0, %0, c0, c1, 4" : "=r" (ia->cpuinfo.ID_MMFR[0]));
      asm("mrc p15, 0, %0, c0, c1, 5" : "=r" (ia->cpuinfo.ID_MMFR[1]));
      asm("mrc p15, 0, %0, c0, c1, 6" : "=r" (ia->cpuinfo.ID_MMFR[2]));
      asm("mrc p15, 0, %0, c0, c1, 7" : "=r" (ia->cpuinfo.ID_MMFR[3]));
      asm("mrc p15, 0, %0, c0, c2, 0" : "=r" (ia->cpuinfo.ID_ISAR[0]));
      asm("mrc p15, 0, %0, c0, c2, 1" : "=r" (ia->cpuinfo.ID_ISAR[1]));
      asm("mrc p15, 0, %0, c0, c2, 2" : "=r" (ia->cpuinfo.ID_ISAR[2]));
      asm("mrc p15, 0, %0, c0, c2, 3" : "=r" (ia->cpuinfo.ID_ISAR[3]));
      asm("mrc p15, 0, %0, c0, c2, 4" : "=r" (ia->cpuinfo.ID_ISAR[4]));
      asm("mrc p15, 0, %0, c0, c2, 5" : "=r" (ia->cpuinfo.ID_ISAR[5]));
    }

  const char *s = l4_kip_version_string(kip);
  if (!s)
    return;

  bool is_hyp_kernel = false;
  l4util_kip_for_each_feature(s)
    if (!strcmp(s, "arm:hyp"))
      {
        if (!running_in_hyp_mode())
          {
            printf("  Detected HYP kernel, switching to HYP mode\n");

            if (   ((ia->cpuinfo.MIDR >> 16) & 0xf) != 0xf // ARMv7
                || (((ia->cpuinfo.ID_PFR[1] >> 12) & 0xf) == 0)) // No Virt Ext
              panic("\nCPU does not support Virtualization Extensions\n");

            if (!plat->arm_switch_to_hyp())
              panic("\nNo switching functionality available on this platform.\n");
            if (!running_in_hyp_mode())
              panic("\nFailed to switch to HYP as required by Fiasco.OC.\n");
          }
        is_hyp_kernel = true;
        break;
      }

  if (!is_hyp_kernel && running_in_hyp_mode())
    {
      printf("  Non-HYP kernel detected but running in HYP mode, switching back.\n");
      asm volatile("mcr p15, 0, sp, c13, c0, 2    \n"
                   "mrs r0, cpsr                  \n"
                   "bic r0, #0x1f                 \n"
                   "orr r0, #0x13                 \n"
                   ".inst 0xe16ef300              \n"  // msr SPSR_hyp, r0
                   "adr r0, 1f                    \n"
                   ".inst 0xe12ef300              \n"  // msr elr_hyp, r0
                   ".inst 0xe160006e              \n"  // eret
                   "1: mrc p15, 0, sp, c13, c0, 2 \n"
                   : : : "r0", "memory");
    }
}
#endif /* arm */

#ifdef ARCH_arm64
static inline unsigned current_el()
{
  l4_umword_t current_el;
  asm ("mrs %0, CurrentEL" : "=r" (current_el));
  return (current_el >> 2) & 3;
}

static void
setup_and_check_kernel_config(Platform_base *, l4_kernel_info_t *kip)
{
  const char *s = l4_kip_version_string(kip);
  if (!s)
    return;

  l4util_kip_for_each_feature(s)
    if (!strcmp(s, "arm:hyp"))
      if (current_el() < 2)
        panic("Kernel requires EL2 (virtualization) but running in EL1.");
}
#endif /* arm64 */

#ifdef ARCH_mips
extern "C" void syncICache(unsigned long start, unsigned long size);
#endif

static unsigned long
load_elf_module(Boot_modules::Module const &mod, char const *n)
{
  printf("  Loading "); print_module_name(mod.cmdline, n); putchar('\n');
  return load_elf_module(mod);
}

/**
 * \brief  Startup, started from crt0.S
 */
/* entry point */
void
startup(char const *cmdline)
{
  if (!cmdline || !*cmdline)
    cmdline = builtin_cmdline;

  if (check_arg(cmdline, "-noserial"))
    set_stdio_uart(NULL);

  if (!Platform_base::platform)
    {
      // will we ever see this?
      printf("No platform found, hangup.");
      while (1)
        ;
    }

  Platform_base *plat = Platform_base::platform;

  if (check_arg(cmdline, "-wait"))
    {
      puts("\nL4 Bootstrapper is waiting for key input to continue...");
      if (getchar() == -1)
        puts("   ...no key input available.");
      else
        puts("    ...going on.");
    }

  puts("\nL4 Bootstrapper");
  puts("  Build: #" BUILD_NR " " BUILD_DATE
#ifdef ARCH_x86
      ", x86-32"
#endif
#ifdef ARCH_amd64
      ", x86-64"
#endif
#ifdef __VERSION__
       ", " __VERSION__
#endif
      );

  if (print_cpu_info)
    print_cpu_info();

  regions.init(__regs, "regions");
  ram.init(__ram, "RAM", get_memory_max_size(cmdline), get_memory_max_address());

  setup_memory_map(cmdline);

  /* basically add the bootstrap binary to the allocated regions */
  init_regions();
  plat->modules()->reserve();

  /* modules to load by bootstrap */
  bool sigma0 = true;   /* we need sigma0 */
  bool roottask = true; /* we need a roottask */

  /* check command line */
  if (check_arg(cmdline, "-no-sigma0"))
    sigma0 = 0;

  if (check_arg(cmdline, "-no-roottask"))
    roottask = 0;

  if (const char *s = check_arg(cmdline, "-modaddr"))
    _mod_addr = RAM_BASE + strtoul(s + 9, 0, 0);

  _mod_addr = l4_round_page(_mod_addr);


  Boot_modules *mods = plat->modules();

  add_elf_regions(mods->module(kernel_module), Region::Kernel);

  if (sigma0)
    add_elf_regions(mods->module(sigma0_module), Region::Sigma0);

  if (roottask)
    add_elf_regions(mods->module(roottask_module), Region::Root);

  l4util_mb_info_t *mbi = plat->modules()->construct_mbi(_mod_addr);

  /* We need at least two boot modules */
  assert(mbi->flags & L4UTIL_MB_MODS);
  /* We have at least the L4 kernel and the first user task */
  assert(mbi->mods_count >= 2);
  assert(mbi->mods_count <= MODS_MAX);

  if (const char *s = cmdline)
    {
      /* patch modules with content given at command line */
      while ((s = check_arg_str((char *)s, "-patch=")))
	patch_module(&s, mbi);
    }

  boot_info_t boot_info;
  l4util_mb_mod_t *mb_mod = (l4util_mb_mod_t *)(unsigned long)mbi->mods_addr;
  regions.optimize();

  /* setup kernel PART ONE */
  boot_info.kernel_start = load_elf_module(mods->module(kernel_module), "[KERNEL]");

  /* setup sigma0 */
  if (sigma0)
    boot_info.sigma0_start = load_elf_module(mods->module(sigma0_module), "[SIGMA0]");

  /* setup roottask */
  if (roottask)
    boot_info.roottask_start = load_elf_module(mods->module(roottask_module),
                                               "[ROOTTASK]");

  /* setup kernel PART TWO (special kernel initialization) */
  void *l4i = find_kip(mods->module(kernel_module));

  regions.optimize();
  regions.dump();

  L4_kernel_options::Options *lko = find_kopts(mods->module(kernel_module), l4i);

  // Note: we have to ensure that the original ELF binaries are not modified
  // or overwritten up to this point. However, the memory regions for the
  // original ELF binaries are freed during load_elf_module() but might be
  // used up to here.
  // ------------------------------------------------------------------------

  // The ELF binaries for the kernel, sigma0, and roottask must no
  // longer be used from here on.
  if (char const *c = check_arg(cmdline, "-presetmem="))
    {
      unsigned fill_value = strtoul(c + 11, NULL, 0);
      fill_mem(fill_value);
    }

  kcmdline_parse(L4_CONST_CHAR_PTR(mb_mod[kernel_module].cmdline), lko);
  lko->uart   = kuart;
  lko->flags |= kuart_flags;


  /* setup the L4 kernel info page before booting the L4 microkernel:
   * patch ourselves into the booter task addresses */
  unsigned long api_version = get_api_version(l4i);
  unsigned major = api_version >> 24;
  switch (major)
    {
    case 0x87: // Fiasco
      init_kip_f(l4i, &boot_info, mbi, &ram, &regions);
      break;
    case 0x02:
      panic("cannot boot V.2 API kernels: %lx\n", api_version);
      break;
    case 0x03:
      panic("cannot boot X.0 and X.1 API kernels: %lx\n", api_version);
      break;
    case 0x84:
      panic("cannot boot Fiasco V.4 API kernels: %lx\n", api_version);
      break;
    case 0x04:
      panic("cannot boot V.4 API kernels: %lx\n", api_version);
    default:
      panic("cannot boot a kernel with unknown api version %lx\n", api_version);
      break;
    }

  printf("  Starting kernel ");
  print_module_name(L4_CONST_CHAR_PTR(mb_mod[kernel_module].cmdline),
		    "[KERNEL]");
  printf(" at " l4_addr_fmt "\n", boot_info.kernel_start);

#if defined(ARCH_arm)
  if (major == 0x87)
    setup_and_check_kernel_config(plat, (l4_kernel_info_t *)l4i);
#endif
#if defined(ARCH_arm64)
  setup_and_check_kernel_config(plat, (l4_kernel_info_t *)l4i);
#endif
#if defined(ARCH_mips)
  {
    printf("  Flushing caches ...\n");
    for (Region *i = ram.begin(); i < ram.end(); ++i)
      {
        if (i->end() >= (512 << 20))
          continue;

        printf("  [%08lx, %08lx)\n", (unsigned long)i->begin(), (unsigned long)i->size());
        syncICache((unsigned long)i->begin(), (unsigned long)i->size());
      }
    printf("  done\n");
  }
#endif
#if defined(ARCH_ppc32)
  init_kip_v2_arch((l4_kernel_info_t*)l4i);

  printf("CPU at %lu Khz/Bus at %lu Hz\n",
         ((l4_kernel_info_t*)l4i)->frequency_cpu,
         ((l4_kernel_info_t*)l4i)->frequency_bus);
#endif

  plat->boot_kernel(boot_info.kernel_start);
  /*NORETURN*/
}

static int
l4_exec_read_exec(void * handle,
		  l4_addr_t file_ofs, l4_size_t file_size,
		  l4_addr_t mem_addr, l4_addr_t /*v_addr*/,
		  l4_size_t mem_size,
		  exec_sectype_t section_type)
{
  Boot_modules::Module const *m = (Boot_modules::Module const *)handle;
  if (!mem_size)
    return 0;

  if (! (section_type & EXEC_SECTYPE_ALLOC))
    return 0;

  if (! (section_type & (EXEC_SECTYPE_ALLOC|EXEC_SECTYPE_LOAD)))
    return 0;

  if (Verbose_load)
    printf("    [%p-%p]\n", (void *) mem_addr, (void *) (mem_addr + mem_size));

  if (!ram.contains(Region::n(mem_addr, mem_addr + mem_size)))
    {
      printf("To be loaded binary region is out of memory region.\n");
      printf(" Binary region: %lx - %lx\n", mem_addr, mem_addr + mem_size);
      dump_ram_map();
      panic("Binary outside memory");
    }

  memcpy((void *) mem_addr, (char const *)m->start + file_ofs, file_size);
  if (file_size < mem_size)
    memset((void *) (mem_addr + file_size), 0, mem_size - file_size);


  Region *f = regions.find(mem_addr);
  if (!f)
    {
      printf("could not find %lx\n", mem_addr);
      regions.dump();
      panic("Oops: region for module not found\n");
    }

  f->name(m->cmdline ? m->cmdline :  ".[Unknown]");
  return 0;
}


static int
l4_exec_add_region(void *handle,
		  l4_addr_t /*file_ofs*/, l4_size_t /*file_size*/,
		  l4_addr_t mem_addr, l4_addr_t v_addr,
		  l4_size_t mem_size,
		  exec_sectype_t section_type)
{
  Elf_info const *info = (Elf_info const *)handle;

  if (!mem_size)
    return 0;

  if (! (section_type & EXEC_SECTYPE_ALLOC))
    return 0;

  if (! (section_type & (EXEC_SECTYPE_ALLOC|EXEC_SECTYPE_LOAD)))
    return 0;

  Region n = Region::n(mem_addr, mem_addr + mem_size,
                       info->mod.cmdline ? info->mod.cmdline : ".[Unknown]",
                       info->type, mem_addr == v_addr ? 1 : 0);

  for (Region *r = regions.begin(); r != regions.end(); ++r)
    if (r->overlaps(n) && r->name() != Boot_modules::Mod_reg)
      {
        printf("  New region for list %s:\t", n.name());
        n.vprint();
        printf("  overlaps with:         \t");
        r->vprint();
        regions.dump();
        panic("region overlap");
      }

  regions.add(Region::n(mem_addr, mem_addr + mem_size,
              info->mod.cmdline ? info->mod.cmdline : ".[Unknown]",
              info->type, mem_addr == v_addr ? 1 : 0), true);
  return 0;
}

static int
l4_exec_find_hdr(void *handle,
                 l4_addr_t /*file_ofs*/, l4_size_t /*file_size*/,
                 l4_addr_t mem_addr, l4_addr_t /*v_addr*/,
                 l4_size_t mem_size,
                 exec_sectype_t section_type)
{
  Hdr_info *hdr = reinterpret_cast<Hdr_info *>(handle);
  if (hdr->hdr_type == (section_type & EXEC_SECTYPE_TYPE_MASK))
    {
      hdr->start = mem_addr;
      hdr->size = mem_size;
      return 1;
    }
  return 0;
}

/*
 * (c) 2008-2009 Frank Mehnert <fm3@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <asm/vm86.h>
#include <asm/atomic.h>

#include <l4linux/x86/l4_memory.h>
#include <l4linux/x86/l4_thread.h>

#include <l4/x86emu/x86emu.h>

#include <l4/sys/types.h>
#include <l4/sys/ipc.h>
#include <l4/sys/kdebug.h>

#undef DBG_IO
#undef DBG_MEM

static u8   FASTCALL(my_inb(X86EMU_pioAddr addr));
static u16  FASTCALL(my_inw(X86EMU_pioAddr addr));
static u32  FASTCALL(my_inl(X86EMU_pioAddr addr));
static void FASTCALL(my_outb(X86EMU_pioAddr addr, u8 val));
static void FASTCALL(my_outw(X86EMU_pioAddr addr, u16 val));
static void FASTCALL(my_outl(X86EMU_pioAddr addr, u32 val));
static u8   FASTCALL(my_rdb(u32 addr));
static u16  FASTCALL(my_rdw(u32 addr));
static u32  FASTCALL(my_rdl(u32 addr));
static void FASTCALL(my_wrb(u32 addr, u8 val));
static void FASTCALL(my_wrw(u32 addr, u16 val));
static void FASTCALL(my_wrl(u32 addr, u32 val));

static unsigned long v_page[1024*1024/(L4_PAGESIZE)];

static void
warn(u32 addr, const char *func)
{
  printk("\033[31mWARNING: Function %s access %08lx\033[m\n", func, addr);
}

static u8
my_inb(X86EMU_pioAddr addr)
{
  int r;
  asm volatile ("inb %w1, %b0" : "=a" (r) : "d" (addr));
#ifdef DBG_IO
  printk("%04x:%04x inb %x -> %x\n", M.x86.R_CS, M.x86.R_IP, addr, r);
  l4_sleep(10);
#endif
  return r;
}

static u16
my_inw(X86EMU_pioAddr addr)
{
  u16 r;
  asm volatile ("inw %w1, %w0" : "=a" (r) : "d" (addr));
#ifdef DBG_IO
  printk("%04x:%04x inw %x -> %x\n", M.x86.R_CS, M.x86.R_IP, addr, r);
  l4_sleep(10);
#endif
  return r;
}

static u32
my_inl(X86EMU_pioAddr addr)
{
  u32 r;
  asm volatile ("inl %w1, %0" : "=a" (r) : "d" (addr));
#ifdef DBG_IO
  printk("%04x:%04x inl %x -> %lx\n", M.x86.R_CS, M.x86.R_IP, addr, r);
  l4_sleep(10);
#endif
  return r;
}

static void
my_outb(X86EMU_pioAddr addr, u8 val)
{
#ifdef DBG_IO
  printk("%04x:%04x outb %x -> %x\n", M.x86.R_CS, M.x86.R_IP, val, addr);
  l4_sleep(10);
#endif
  asm volatile ("outb %b0, %w1" : "=a" (val), "=d" (addr)
                                : "a" (val), "d" (addr));
}

static void
my_outw(X86EMU_pioAddr addr, u16 val)
{
#ifdef DBG_IO
  printk("%04x:%04x outw %x -> %x\n", M.x86.R_CS, M.x86.R_IP, val, addr);
  l4_sleep(10);
#endif
  asm volatile ("outw %w0, %w1" : "=a" (val), "=d" (addr)
                                : "a" (val), "d" (addr));
}

static void
my_outl(X86EMU_pioAddr addr, u32 val)
{
#ifdef DBG_IO
  printk("%04x:%04x outl %lx -> %x\n", M.x86.R_CS, M.x86.R_IP, val, addr);
  l4_sleep(10);
#endif
  asm volatile ("outl %0, %w1" : "=a"(val), "=d" (addr) 
                               : "a" (val), "d" (addr));
}

static u8
my_rdb(u32 addr)
{
  if (addr > 1 << 20)
    warn(addr, __FUNCTION__);

#ifdef DBG_MEM
  printk("readb %08lx->%08lx => %02x\n",
      addr, v_page[addr/L4_PAGESIZE] + (addr % L4_PAGESIZE), 
      *(u8*)(v_page[addr/L4_PAGESIZE] + (addr % L4_PAGESIZE)));
  l4_sleep(10);
#endif
  return *(u32*)(v_page[addr/L4_PAGESIZE] + (addr % L4_PAGESIZE));
}

static u16
my_rdw(u32 addr)
{
  if (addr > 1 << 20)
    warn(addr, __FUNCTION__);

#ifdef DBG_MEM
  printk("readw %08lx->%08lx => %04x\n",
      addr, v_page[addr/L4_PAGESIZE] + (addr % L4_PAGESIZE),
      *(u16*)(v_page[addr/L4_PAGESIZE] + (addr % L4_PAGESIZE)));
  l4_sleep(10);
#endif
  return *(u16*)(v_page[addr/L4_PAGESIZE] + (addr % L4_PAGESIZE));
}

static u32
my_rdl(u32 addr)
{
  if (addr > 1 << 20)
    warn(addr, __FUNCTION__);

#ifdef DBG_MEM
  printk("readl %08lx->%08lx => %08lx\n",
      addr, v_page[addr/L4_PAGESIZE] + (addr % L4_PAGESIZE), 
      *(u32*)(v_page[addr/L4_PAGESIZE] + (addr % L4_PAGESIZE)));
  l4_sleep(10);
#endif
  return *(u32*)(v_page[addr/L4_PAGESIZE] + (addr % L4_PAGESIZE));
}

static void 
my_wrb(u32 addr, u8 val)
{
  if (addr > 1 << 20)
    warn(addr, __FUNCTION__);

#ifdef DBG_MEM
  printk("writeb %08lx->%08lx => %02x\n",
      addr, v_page[addr/L4_PAGESIZE] + (addr % L4_PAGESIZE), val);
  l4_sleep(10);
#endif
  *(u8*)(v_page[addr/L4_PAGESIZE] + (addr % L4_PAGESIZE)) = val;
}

static void 
my_wrw(u32 addr, u16 val)
{
  if (addr > 1 << 20)
    warn(addr, __FUNCTION__);

#ifdef DBG_MEM
  printk("writew %08lx->%08lx => %04x\n",
      addr, v_page[addr/L4_PAGESIZE] + (addr % L4_PAGESIZE), val);
  l4_sleep(10);
#endif
  *(u16*)(v_page[addr/L4_PAGESIZE] + (addr % L4_PAGESIZE)) = val;
}

static void 
my_wrl(u32 addr, u32 val)
{
  if (addr > 1 << 20)
    warn(addr, __FUNCTION__);

#ifdef DBG_MEM
  printk("writel %08lx->%08lx => %08lx\n",
      addr, v_page[addr/L4_PAGESIZE] + (addr % L4_PAGESIZE), val);
  l4_sleep(10);
#endif
  *(u32*)(v_page[addr/L4_PAGESIZE] + (addr % L4_PAGESIZE)) = val;
}

X86EMU_pioFuncs my_pioFuncs =
{
  my_inb,  my_inw,  my_inl,
  my_outb, my_outw, my_outl
};

X86EMU_memFuncs my_memFuncs =
{
  my_rdb, my_rdw, my_rdl,
  my_wrb, my_wrw, my_wrl
};


static int
do_x86_emu(struct vm86_struct *info)
{
  int ret;
  unsigned addr;
  static atomic_t do_x86_emu_lock = ATOMIC_INIT(0);

  /* X86EMU is not reenrant */
  if (!test_and_set_bit(0, &do_x86_emu_lock))
    {
      MOD_INC_USE_COUNT;
      
      M.x86.R_EAX  = info->regs.eax;
      M.x86.R_EBX  = info->regs.ebx;
      M.x86.R_ECX  = info->regs.ecx;
      M.x86.R_EDX  = info->regs.edx;
      M.x86.R_ESI  = info->regs.esi;
      M.x86.R_EDI  = info->regs.edi;
      M.x86.R_EBP  = info->regs.ebp;
      M.x86.R_ESP  = info->regs.esp;
      M.x86.R_EIP  = info->regs.eip;
      M.x86.R_EFLG = info->regs.eflags;
      M.x86.R_CS   = info->regs.cs;
      M.x86.R_DS   = info->regs.ds;
      M.x86.R_ES   = info->regs.es;
      M.x86.R_SS   = info->regs.ss;
      M.x86.R_FS   = info->regs.fs;
      M.x86.R_GS   = info->regs.gs;
 
#if 0
      printk(">> Before x86 emulator:\n"
	     "   eax=%08lx ebx=%08lx ecx=%08lx edx=%08lx\n"
	     "   esi=%08lx edi=%08lx ebp=%08lx esp=%08lx\n"
	     "   cs=%04x  ds=%04x  es=%04x  ss=%04x\n"
	     "   eip=%08lx eflags=%08lx v_page=%08x\n",
	     info->regs.eax, info->regs.ebx, info->regs.ecx, info->regs.edx,
	     info->regs.esi, info->regs.edi, info->regs.ebp, info->regs.esp,
	     info->regs.cs,  info->regs.ds,  info->regs.es,  info->regs.ss,
	     info->regs.eip, info->regs.eflags, (unsigned)v_page);
#endif
      
      /* The client has mapped the complete 1MB address space */
      for (addr=0; addr<(1024*1024); addr+=L4_PAGESIZE)
	{
	  unsigned long dummy_offset;
	  
	  v_page[addr/L4_PAGESIZE] = l4_get_user_page(addr, &dummy_offset);
	}
  
      /* execute emulator */
      X86EMU_exec();
  
#if 0
      printk("<< After x86 emulator:\n"
	     "   eax=%08lx ebx=%08lx ecx=%08lx edx=%08lx\n"
	     "   esi=%08lx edi=%08lx ebp=%08lx esp=%08lx\n"
	     "   cs=%04x  ds=%04x  es=%04x  ss=%04x\n"
	     "   eip=%08lx eflags=%08lx\n",
	     M.x86.R_EAX, M.x86.R_EBX, M.x86.R_ECX, M.x86.R_EDX,
	     M.x86.R_ESI, M.x86.R_EDI, M.x86.R_EBP, M.x86.R_ESP,
	     M.x86.R_CS,  M.x86.R_DS,  M.x86.R_ES,  M.x86.R_SS,
	     M.x86.R_EIP, M.x86.R_EFLG);
#endif
      
      info->regs.eax    = M.x86.R_EAX;
      info->regs.ebx    = M.x86.R_EBX;
      info->regs.ecx    = M.x86.R_ECX;
      info->regs.edx    = M.x86.R_EDX;
      info->regs.esi    = M.x86.R_ESI;
      info->regs.edi    = M.x86.R_EDI;
      info->regs.ebp    = M.x86.R_EBP;
      info->regs.esp    = M.x86.R_ESP;
      info->regs.eip    = M.x86.R_EIP;
      info->regs.eflags = M.x86.R_EFLG;
      info->regs.cs     = M.x86.R_CS;
      info->regs.ds     = M.x86.R_DS;
      info->regs.es     = M.x86.R_ES;
      info->regs.ss     = M.x86.R_SS;
      info->regs.fs     = M.x86.R_FS;
      info->regs.gs     = M.x86.R_GS;

      MOD_DEC_USE_COUNT;

      clear_bit(0, &do_x86_emu_lock);

      /* make sure that EIP points to the HLT instruction */
      info->regs.eip--;
      ret = VM86_UNKNOWN;
    }
  else
    {
      printk("x86 emulator called again\n");
      ret = -EPERM;
    }

  /* VM86_UNKNOWN means that we got an exception. This was expected
   * since the code to emulate ends with HLT */
  return ret;
}

static int
init_emu_module(void)
{
  M.x86.debug = 0 /*| DEBUG_DECODE_F*/;
  
  X86EMU_setupPioFuncs(&my_pioFuncs);
  X86EMU_setupMemFuncs(&my_memFuncs);

  x86_emu = do_x86_emu;

  return 0;
}

static void
done_emu_module(void)
{
  x86_emu = 0;
}

module_init(init_emu_module);
module_exit(done_emu_module);


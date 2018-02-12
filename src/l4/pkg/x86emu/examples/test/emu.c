/*
 * (c) 2008-2009 Frank Mehnert <fm3@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/x86emu/x86emu.h>

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include <l4/sys/types.h>
#include <l4/sys/ipc.h>
#include <l4/sys/kdebug.h>
#include <l4/util/rdtsc.h>

void
printk(const char *format,...)
{
  va_list list;
  va_start(list, format);
  vprintf(format, list);
  va_end(list);
}

//#define TEST

static unsigned emu_mem;

static u8   X86API my_inb(X86EMU_pioAddr addr);
static u16  X86API my_inw(X86EMU_pioAddr addr);
static u32  X86API my_inl(X86EMU_pioAddr addr);
static void X86API my_outb(X86EMU_pioAddr addr, u8 val);
static void X86API my_outw(X86EMU_pioAddr addr, u16 val);
static void X86API my_outl(X86EMU_pioAddr addr, u32 val);
static u8   X86API my_rdb(u32 addr);
static u16  X86API my_rdw(u32 addr);
static u32  X86API my_rdl(u32 addr);
static void X86API my_wrb(u32 addr, u8 val);
static void X86API my_wrw(u32 addr, u16 val);
static void X86API my_wrl(u32 addr, u32 val);


static u8
X86API my_inb(X86EMU_pioAddr addr)
{
  int r;
  asm volatile ("inb %w1, %b0" : "=a" (r) : "d" (addr));
  return r;
}

static u16
X86API my_inw(X86EMU_pioAddr addr)
{
  u16 r;
  asm volatile ("inw %w1, %w0" : "=a" (r) : "d" (addr));
  return r;
}

static u32
X86API my_inl(X86EMU_pioAddr addr)
{
  u32 r;
  asm volatile ("inl %w1, %0" : "=a" (r) : "d" (addr));
  return r;
}

static void
X86API my_outb(X86EMU_pioAddr addr, u8 val)
{
//  printf("%04x:%04x outb %x -> %x\n", M.x86.R_CS, M.x86.R_IP, val, addr);
  asm volatile ("outb %b0, %w1" : "=a" (val), "=d" (addr)
                                : "a" (val), "d" (addr));
}

static void
X86API my_outw(X86EMU_pioAddr addr, u16 val)
{
//  printf("%04x:%04x outw %x -> %x\n", M.x86.R_CS, M.x86.R_IP, val, addr);
  asm volatile ("outw %w0, %w1" : "=a" (val), "=d" (addr)
                                : "a" (val), "d" (addr));
}

static void
X86API my_outl(X86EMU_pioAddr addr, u32 val)
{
//  printf("%04x:%04x outl %x -> %x\n", M.x86.R_CS, M.x86.R_IP, val, addr);
  asm volatile ("outl %0, %w1" : "=a"(val), "=d" (addr) 
                               : "a" (val), "d" (addr));
}

static u8
X86API my_rdb(u32 addr)
{
  return *(u32*)(M.mem_base + addr);
}

static u16
X86API my_rdw(u32 addr)
{
  return *(u16*)(M.mem_base + addr);
}

static u32
X86API my_rdl(u32 addr)
{
  return *(u32*)(M.mem_base + addr);
}

static void 
X86API my_wrb(u32 addr, u8 val)
{
  *(u8*)(M.mem_base + addr) = val;
}

static void 
X86API my_wrw(u32 addr, u16 val)
{
  *(u16*)(M.mem_base + addr) = val;
}

static void 
X86API my_wrl(u32 addr, u32 val)
{
  *(u32*)(M.mem_base + addr) = val;
}

X86EMU_pioFuncs my_pioFuncs =
{
  my_inb,
  my_inw,
  my_inl,
  my_outb,
  my_outw,
  my_outl
};

X86EMU_memFuncs my_memFuncs =
{
  my_rdb,
  my_rdw,
  my_rdl,
  my_wrb,
  my_wrw,
  my_wrl
};

static void
map_emu_page(unsigned get_address, unsigned map_address)
{
  int error;
  l4_umword_t dummy;
  l4_msgdope_t result;
  
  error = l4_ipc_call(rmgr_pager_id,
		      L4_IPC_SHORT_MSG, get_address, 0,
		      L4_IPC_MAPMSG((l4_umword_t)emu_mem+map_address,
				    L4_LOG2_PAGESIZE),
		      &dummy, &dummy,
		      L4_IPC_NEVER, &result);
  if (error)
    {
      printf("Error %02x receiving page at %08x\n", error, get_address);
      exit(-1);
    }
  if (!l4_ipc_fpage_received(result))
    {
      printf("Can't receive page at %08x\n", get_address);
      exit(-1);
    }
}

int 
main(int argc, char **argv)
{
#define EMU_MEM_SIZE (16*1024)

  extern void *_end;
  int error;
  l4_umword_t address;
#ifdef TEST
  l4_cpu_time_t start, stop;
#endif

  rmgr_init();
  l4_calibrate_tsc();

  emu_mem = (((l4_umword_t)&_end)+2*L4_PAGESIZE-1) & L4_PAGEMASK;
  
  printf("emu_mem = %08x\n", emu_mem);

  /* build 16-bit address space starting at emu_mem */

  /* map bios data segment */
  if ((error = rmgr_get_page0((void*)emu_mem)))
    {
      printf("Error %02x mapping page 0\n", error & 0xff);
      exit(-1);
    }

  /* map VGA bios code segment */
  for (address=0xC0000; address < 0xD0000; address += L4_PAGESIZE)
    map_emu_page(address, address);

  /* map system BIOS code segment */
  for (address=0xF0000; address < 0x100000; address += L4_PAGESIZE)
    map_emu_page(address, address);

  /* map some page for code */
  map_emu_page(0xfffffffc, L4_PAGESIZE);

  /* int 10 */
  *((unsigned char*)emu_mem+L4_PAGESIZE  ) = 0xcd;
  *((unsigned char*)emu_mem+L4_PAGESIZE+1) = 0x10;
  /* hlt */
  *((unsigned char*)emu_mem+L4_PAGESIZE+2) = 0xf4;

  M.mem_base = emu_mem;
  M.mem_size = 1024*1024;
  M.x86.debug = 0;
  
  X86EMU_setupPioFuncs(&my_pioFuncs);
  X86EMU_setupMemFuncs(&my_memFuncs);

#ifdef TEST

  M.x86.R_AX = 0x4F02;
  M.x86.R_BX = 0x114;
  M.x86.R_SP = L4_PAGESIZE;
  M.x86.R_IP = 0;
  M.x86.R_CS = L4_PAGESIZE >> 4;
  M.x86.R_DS = M.x86.R_CS;
  M.x86.R_ES = M.x86.R_CS;
  M.x86.R_SS = M.x86.R_CS;

  printf("Starting emulator\n");

  start = l4_rdtsc();
  X86EMU_exec();
  stop  = l4_rdtsc();

  stop -= start;
  
  printf("Stopping emulator (time was %d ms)\n",
      ((unsigned int)l4_tsc_to_ns(stop))/1000000);

#else

    {
      l4_threadid_t sender;
      l4_msgdope_t result;
      struct
	{
	  l4_fpage_t fp;
	  l4_msgdope_t size_dope;
	  l4_msgdope_t send_dope;
	  l4_umword_t dw[12];
	} msg;

#if 0
      if (!names_register("X86EMU"))
	{
	  printf("Error registering at names\n");
	  exit(-1);
	}
#endif
	  
      for (;;)
	{
	  msg.size_dope = L4_IPC_DOPE(12, 0);
	  msg.send_dope = L4_IPC_DOPE(12, 0);
  
	  error = l4_ipc_wait(&sender, 
			      &msg, &msg.dw[0], &msg.dw[1],
			      L4_IPC_NEVER, &result);

	  while (!error)
	    {
	      M.x86.R_EAX = msg.dw[0];
	      M.x86.R_EBX = msg.dw[1];
	      M.x86.R_ECX = msg.dw[2];
	      M.x86.R_EDX = msg.dw[3];
	      M.x86.R_ESI = msg.dw[4];
	      M.x86.R_EDI = msg.dw[5];
	      M.x86.R_EBP = msg.dw[6];
	      M.x86.R_EIP = msg.dw[7];
	      M.x86.R_EFLG = msg.dw[8];
	  
	      M.x86.R_CS = msg.dw[9];
	      M.x86.R_DS = msg.dw[10];
	      M.x86.R_ES = msg.dw[11];
	  
	      M.x86.R_ESP = L4_PAGESIZE;
	      M.x86.R_SS = 0x0100;
  
	      printf("Starting emulator:\n"
		     "eax=%08lx ebx=%08lx ecx=%08lx edx=%08lx\n"
		     "esi=%08lx edi=%08lx ebp=%08lx esp=%08lx\n"
		     "eip=%08lx eflags=%08lx\n"
		     "cs=%04x ds=%04x es=%04x ss=%04x\n",
		     M.x86.R_EAX, M.x86.R_EBX, M.x86.R_ECX, M.x86.R_EDX,
		     M.x86.R_ESI, M.x86.R_EDI, M.x86.R_EBP, M.x86.R_ESP,
		     M.x86.R_EIP, M.x86.R_EFLG,
		     M.x86.R_CS, M.x86.R_DS, M.x86.R_ES, M.x86.R_SS);

	      enter_kdebug("stop");

#if 0
	      start = l4_rdtsc();
	      X86EMU_exec();
	      stop  = l4_rdtsc();

	      stop -= start;
  
	      printf("Stopping emulator (time was %d ms)\n",
		  ((unsigned int)l4_tsc_to_ns(stop))/1000000);
#endif

	      msg.dw[0] = M.x86.R_EAX;
	      msg.dw[1] = M.x86.R_EBX;
	      msg.dw[2] = M.x86.R_ECX;
	      msg.dw[3] = M.x86.R_EDX;
	      msg.dw[4] = M.x86.R_ESI;
	      msg.dw[5] = M.x86.R_EDI;
	      msg.dw[6] = M.x86.R_EBP;
	      msg.dw[7] = M.x86.R_EIP;
	      msg.dw[8] = M.x86.R_EFLG;
	  
	      msg.dw[9] = M.x86.R_CS;
	      msg.dw[10] = M.x86.R_DS;
	      msg.dw[11] = M.x86.R_ES;
	      
	      error = l4_ipc_reply_and_wait(sender, 
					    &msg, msg.dw[0], msg.dw[1],
					    &sender,
					    &msg, &msg.dw[0], &msg.dw[1],
					    L4_IPC_SEND_TIMEOUT_0,
					    &result);
	    }
	}
    }
  
#endif
  
  return 0;
}


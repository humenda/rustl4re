/* (c) 2003 'Technische Universitaet Dresden'
 * This file is part of the con package, which is distributed under
 * the terms of the GNU General Public License 2. Please see the
 * COPYING file for details. */

#include <stdio.h>
#include <l4/sys/types.h>
#include <l4/names/libnames.h>
#include <l4/l4rm/l4rm.h>
#include <l4/util/rdtsc.h>
#include <l4/util/irq.h>
#include <l4/l4con/l4con.h>
#include <l4/thread/thread.h>
#include <l4/dm_mem/dm_mem.h>
#include <l4/dm_phys/dm_phys.h>
#include <l4/l4con/l4con-client.h>

char LOG_tag[9] = "conbench";

static l4_threadid_t con_l4id;
static l4_threadid_t vc_l4id;

#define ROUNDS 40

#define MEM(addr) 							\
	asm volatile ("1: movl %%eax, (%%edi)	\n\t"			\
	              "   movl %%eax, 4(%%edi)	\n\t"			\
	              "   movl %%eax, 8(%%edi)	\n\t"			\
	              "   movl %%eax, 12(%%edi)	\n\t"			\
  		      "   add  $16, %%edi	\n\t"			\
		      "   dec  %%ecx		\n\t"			\
		      "   jnz  1b		\n\t"			\
		      : "=c"(dummy), "=D"(dummy)			\
		      : "a"(0xaaaaaaaa), "c"(1024*1024/16), "D"(addr))

int
main(int argc, char **argv)
{
  CORBA_Environment _env = dice_default_environment;
  int error, i;
  l4_addr_t map_addr, write_addr;
  l4_uint32_t map_area;
  l4_offs_t offset;
  l4_snd_fpage_t snd_fpage;
  l4_cpu_time_t start, stop;
  unsigned int dummy;
  unsigned int us;
  unsigned kb, mb;
  void *ram;

  if (!(ram = l4dm_mem_allocate(L4_SUPERPAGESIZE,
				L4DM_CONTIGUOUS|L4DM_PINNED|
				L4RM_MAP|L4RM_LOG2_ALIGNED|
				L4DM_MEMPHYS_SUPERPAGES)))
    {
      printf("Error reserving %dMB RAM\n", L4_SUPERPAGESIZE/(1024*1024));
      return -1;
    }

  if (!names_waitfor_name(CON_NAMES_STR, &con_l4id, 5000))
    {
      printf("con not found\n");
      return -1;
    }

  if ((error = con_if_openqry_call(&con_l4id, 65536, 0, 0, L4THREAD_DEFAULT_PRIO,
			     &vc_l4id, CON_NOVFB, &_env)))
    {
      printf("Error %d opening vc\n", error);
      return error;
    }

  if ((error = l4rm_area_reserve(L4_SUPERPAGESIZE,
				 L4RM_LOG2_ALIGNED,
				 &map_addr,
				 &map_area)))
    {
      printf("Error %d reserving map area\n", error);
      return error;
    }

  _env.rcv_fpage = l4_fpage(map_addr, L4_LOG2_SUPERPAGESIZE, 0, 0);
  if (con_vc_graph_mapfb_call(&vc_l4id, 0, 
                              &snd_fpage, &offset, &_env)
      || DICE_HAS_EXCEPTION(&_env))
    {
      printf("Error mapping framebuffer (exc=%d)", DICE_EXCEPTION_MAJOR(&_env));
      return -1;
    }

  write_addr = map_addr + offset;

  printf("ram at %08lx, fb at %08lx\n", (unsigned long)ram, write_addr);

  l4_calibrate_tsc();

  l4util_cli();
  start = l4_rdtsc();

  for (i=0; i<ROUNDS; i++)
    MEM(ram);

  stop  = l4_rdtsc();
  l4util_sti();

  stop -= start;
  
  us = (unsigned)l4_tsc_to_ns(stop)/1000;
  kb = (ROUNDS*10000*1024) / (us/100);
  mb = kb/1000;
  kb -= mb*1000;

  printf("ram: %11d cycles, %5dms, %4d.%03dMB/s\n",
         (l4_uint32_t)stop, us/1000, mb, kb);

  l4util_cli();
  start = l4_rdtsc();

  for (i=ROUNDS; i>0; i--)
    MEM(write_addr);

  stop  = l4_rdtsc();
  l4util_sti();

  stop -= start;
  
  us = (unsigned)l4_tsc_to_ns(stop)/1000;
  kb = (ROUNDS*10000*1024) / (us/100);
  mb = kb/1000;
  kb -= mb*1000;

  printf("fb:  %11d cycles, %5dms, %4d.%03dMB/s\n",
         (l4_uint32_t)stop, us/1000, mb, kb);

  return 0;
}


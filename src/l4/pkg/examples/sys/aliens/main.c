/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>,
 *               Björn Döbel <doebel@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
/*
 * Example to show syscall tracing.
 */
#if defined(ARCH_x86) || defined(ARCH_amd64)
// MEASURE only works on x86/amd64
//#define MEASURE
#endif

#include <l4/sys/ipc.h>
#include <l4/sys/thread.h>
#include <l4/sys/factory.h>
#include <l4/sys/utcb.h>
#include <l4/util/util.h>
#include <l4/re/env.h>
#include <l4/re/c/util/cap_alloc.h>
#include <l4/re/c/util/kumem_alloc.h>
#include <l4/sys/debugger.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Architecture specifics */
#if defined(ARCH_x86) || defined(ARCH_amd64)

static int
is_alien_after_call(l4_exc_regs_t const *exc)
{
#if defined(ARCH_x86)
  return exc->err & 4;
#else
  return exc->err == 1;
#endif
}

static inline void
_print_exc_state(l4_exc_regs_t const *exc)
{
  printf("PC=%08lx SP=%08lx Err=%08lx Trap=%lx, %s syscall, SC-Nr: %lx\n",
         l4_utcb_exc_pc(exc), exc->sp, exc->err,
         exc->trapno, is_alien_after_call(exc) ? " after" : "before",
         exc->err >> 3);
}

#elif defined(ARCH_arm)

static int
is_alien_after_call(l4_exc_regs_t const *exc)
{ return exc->err & 0x40; } // TODO: Should change this to (1 << 16)

static inline void
_print_exc_state(l4_exc_regs_t const *exc)
{
  printf("PC=%08lx SP=%08lx ULR=%08lx CPSR=%08lx Err=%lx/%lx, %s syscall\n",
         l4_utcb_exc_pc(exc), exc->sp, exc->ulr, exc->cpsr,
         exc->err, exc->err >> 26,
         is_alien_after_call(exc) ? " after" : "before");
}

#elif defined(ARCH_arm64)

static int
is_alien_after_call(l4_exc_regs_t const *exc)
{ return exc->err & (1ul << 16); }

static inline void
_print_exc_state(l4_exc_regs_t const *exc)
{
  printf("PC=%08lx SP=%08lx PSTATE=%08lx Err=%lx/%lx, %s syscall\n",
         l4_utcb_exc_pc(exc), exc->sp, exc->pstate,
         exc->err, exc->err >> 26,
         is_alien_after_call(exc) ? " after" : "before");
}

#elif defined(ARCH_mips)

static int
is_alien_after_call(l4_exc_regs_t const *exc)
{ return 0; }

static inline void
_print_exc_state(l4_exc_regs_t const *exc)
{
  printf("PC=%08lx SP=%08lx Cause=%lx, %s syscall\n",
         l4_utcb_exc_pc(exc), exc->sp, exc->cause,
         is_alien_after_call(exc) ? " after" : "before");
}

#else

static int
is_alien_after_call(l4_exc_regs_t const *exc)
{ return exc->err & 1; }

static inline void
_print_exc_state(l4_exc_regs_t const *exc)
{
  printf("PC=%08lx SP=%08lx, %s syscall\n",
         l4_utcb_exc_pc(exc), exc->sp,
         is_alien_after_call(exc) ? " after" : "before");
}

#endif

/* Measurement mode specifics.
 *
 * In measurement mode the code is less verbose and uses RDTSC for alient exception
 * performance measurement.
 */
#ifdef MEASURE

#include <l4/util/rdtsc.h>

static inline void
calibrate_timer(void)
{
  l4_calibrate_tsc(l4re_kip());
}

static inline void
print_timediff(l4_cpu_time_t start)
{
  e = l4_rdtsc();
  printf("time %lld\n", l4_tsc_to_ns(e - start));
}

static inline void
alien_sleep(void)
{
  l4_sleep(0);
}

static inline void
print_exc_state(l4_exc_regs_t const *exc)
{
  if (0)
    _print_exc_state(exc);
}

#else

static inline void
calibrate_timer(void)
{
}

static inline void
print_timediff(l4_cpu_time_t start)
{
  (void)start;
}

static inline l4_cpu_time_t
l4_rdtsc(void)
{
  return 0;
}

static inline void
alien_sleep(void)
{
  l4_sleep(1000);
}

static inline void
print_exc_state(l4_exc_regs_t const *exc)
{
  _print_exc_state(exc);
}

#endif


static char alien_thread_stack[8 << 10];
static l4_cap_idx_t alien;

static void alien_thread(void)
{
  while (1)
    {
      l4_ipc_call(0x1234 << L4_CAP_SHIFT, l4_utcb(),
                  l4_msgtag(0, 0, 0, 0), L4_IPC_NEVER);
      alien_sleep();
    }
}

int main(void)
{
  l4_msgtag_t tag;
  l4_cpu_time_t s;
  l4_utcb_t *u = l4_utcb();
  l4_exc_regs_t exc;
  l4_umword_t mr0, mr1;

  printf("Alien feature testing\n");

  l4_debugger_set_object_name(l4re_env()->main_thread, "alientest");

  /* Start alien thread */
  if (l4_is_invalid_cap(alien = l4re_util_cap_alloc()))
    return 1;

  l4_touch_rw(alien_thread_stack, sizeof(alien_thread_stack));

  tag = l4_factory_create_thread(l4re_env()->factory, alien);
  if (l4_error(tag))
    return 2;

  l4_debugger_set_object_name(alien, "alienth");

  l4_addr_t kumem;
  if (l4re_util_kumem_alloc(&kumem, 0, L4RE_THIS_TASK_CAP, l4re_env()->rm))
    return 3;

  l4_thread_control_start();
  l4_thread_control_pager(l4re_env()->main_thread);
  l4_thread_control_exc_handler(l4re_env()->main_thread);
  l4_thread_control_bind((l4_utcb_t *)kumem, L4RE_THIS_TASK_CAP);
  l4_thread_control_alien(1);
  tag = l4_thread_control_commit(alien);
  if (l4_error(tag))
    return 4;

  tag = l4_thread_ex_regs(alien,
                          (l4_umword_t)alien_thread,
                          (l4_umword_t)alien_thread_stack + sizeof(alien_thread_stack),
                          0);
  if (l4_error(tag))
    return 5;

  l4_sched_param_t sp = l4_sched_param(1, 0);
  tag = l4_scheduler_run_thread(l4re_env()->scheduler, alien, &sp);
  if (l4_error(tag))
    return 6;

  calibrate_timer();

  /* Pager/Exception loop */
  if (l4_msgtag_has_error(tag = l4_ipc_receive(alien, u, L4_IPC_NEVER)))
    {
      printf("l4_ipc_receive failed");
      return 7;
    }

  memcpy(&exc, l4_utcb_exc(), sizeof(exc));
  mr0 = l4_utcb_mr()->mr[0];
  mr1 = l4_utcb_mr()->mr[1];

  for (;;)
    {
      s = l4_rdtsc();

      if (l4_msgtag_is_exception(tag))
        {
          print_exc_state(&exc);
          tag = l4_msgtag(is_alien_after_call(&exc)
                           ? 0 : L4_PROTO_ALLOW_SYSCALL,
                          L4_UTCB_EXCEPTION_REGS_SIZE, 0, 0);
        }
      else
        printf("Umm, non-handled request (like PF): %lx %lx\n", mr0, mr1);

      memcpy(l4_utcb_exc(), &exc, sizeof(exc));

      /* Reply and wait */
      if (l4_msgtag_has_error(tag = l4_ipc_call(alien, u, tag, L4_IPC_NEVER)))
        {
          printf("l4_ipc_call failed\n");
          return 8;
        }
      memcpy(&exc, l4_utcb_exc(), sizeof(exc));
      mr0 = l4_utcb_mr()->mr[0];
      mr1 = l4_utcb_mr()->mr[1];
      print_timediff(s);
    }

  return 0;
}

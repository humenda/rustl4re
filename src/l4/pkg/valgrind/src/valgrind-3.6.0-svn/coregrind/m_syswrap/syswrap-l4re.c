/*--------------------------------------------------------------------*/
/*--- L4RE-specific syscalls, etc.                  syswrap-l4re.c ---*/
/*--------------------------------------------------------------------*/

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2000-2008 Nicholas Nethercote
      njn@valgrind.org

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307, USA.

   The GNU General Public License is contained in the file COPYING.
*/

#include "pub_core_basics.h"
#include "pub_core_vki.h"
#include "pub_core_vkiscnums.h"
#include "pub_core_libcsetjmp.h"
#include "pub_core_threadstate.h"
#include "pub_core_aspacemgr.h"
#include "pub_core_debuginfo.h"    // VG_(di_notify_*)
#include "pub_core_transtab.h"     // VG_(discard_translations)
#include "pub_core_xarray.h"
#include "pub_core_clientstate.h"
#include "pub_core_debuglog.h"
#include "pub_core_libcbase.h"
#include "pub_core_libcassert.h"
#include "pub_core_libcfile.h"
#include "pub_core_libcprint.h"
#include "pub_core_libcproc.h"
#include "pub_core_libcsignal.h"
#include "pub_core_mallocfree.h"
#include "pub_core_tooliface.h"
#include "pub_core_options.h"
#include "pub_core_scheduler.h"
#include "pub_core_signals.h"
#include "pub_core_syscall.h"
#include "pub_core_syswrap.h"

#include "priv_types_n_macros.h"
#include "priv_syswrap-generic.h"
#include "priv_syswrap-l4re.h"


// Run a thread from beginning to end and return the thread's
// scheduler-return-code.
static VgSchedReturnCode thread_wrapper(Word /*ThreadId*/ tidW)
{
   VgSchedReturnCode ret;
   ThreadId     tid = (ThreadId)tidW;
   ThreadState* tst = VG_(get_ThreadState)(tid);
   VG_(debugLog)(1, "syswrap-l4re", 
                    "thread_wrapper(tid=%lld): entry\n", 
                    (ULong)tidW);

   vg_assert(tst->status == VgTs_Init);

   /* make sure we get the CPU lock before doing anything significant */
   VG_(acquire_BigLock)(tid, "thread_wrapper(starting new thread)");

   if (0)
      VG_(printf)("thread tid %d started: stack = %p\n",
		  tid, &tid);

   VG_TRACK(pre_thread_first_insn, tid);

   tst->os_state.lwpid = VG_(gettid)();
   tst->os_state.threadgroup = VG_(getpid)();

   /* Thread created with all signals blocked; scheduler will set the
      appropriate mask */
#if defined(VGO_l4re)
   {
	   ThreadState* ctst = VG_(get_ThreadState)(tid);
	   VG_(debugLog)(1, "syswrap", "register contents a startup tid=%d\n", tid);	
	   VG_(debugLog)(1, "syswrap", "eax = %x\n", ctst->arch.vex.guest_EAX);
	   VG_(debugLog)(1, "syswrap", "ebx = %x\n", ctst->arch.vex.guest_EBX);
	   VG_(debugLog)(1, "syswrap", "ecx = %x\n", ctst->arch.vex.guest_ECX);
	   VG_(debugLog)(1, "syswrap", "edx = %x\n", ctst->arch.vex.guest_EDX);
	   VG_(debugLog)(1, "syswrap", "esi = %x\n", ctst->arch.vex.guest_ESI);
	   VG_(debugLog)(1, "syswrap", "edi = %x\n", ctst->arch.vex.guest_EDI);
	   VG_(debugLog)(1, "syswrap", "ebp = %x\n", ctst->arch.vex.guest_EBP);
	   VG_(debugLog)(1, "syswrap", "esp = %x\n", ctst->arch.vex.guest_ESP);

   }
#endif

   ret = VG_(scheduler)(tid);

   vg_assert(VG_(is_exiting)(tid));
   
   vg_assert(tst->status == VgTs_Runnable);
   vg_assert(VG_(is_running_thread)(tid));

   VG_(debugLog)(1, "syswrap-l4re", 
                    "thread_wrapper(tid=%lld): exit\n", 
                    (ULong)tidW);

   /* Return to caller, still holding the lock. */
   return ret;
}


/* ---------------------------------------------------------------------
   clone-related stuff
   ------------------------------------------------------------------ */

/* Run a thread all the way to the end, then do appropriate exit actions
   (this is the last-one-out-turn-off-the-lights bit).  */
static void run_a_thread_NORETURN ( Word tidW )
{
   ThreadId          tid = (ThreadId)tidW;
   VgSchedReturnCode src;
   Int               c;

   VG_(debugLog)(1, "syswrap-l4re", 
                    "run_a_thread_NORETURN(tid=%lld): pre-thread_wrapper\n",
                    (ULong)tidW);

   /* Run the thread all the way through. */
   src = thread_wrapper(tid);  

   VG_(debugLog)(1, "syswrap-l4re", 
                    "run_a_thread_NORETURN(tid=%lld): post-thread_wrapper\n",
                    (ULong)tidW);

   c = VG_(count_living_threads)();
   vg_assert(c >= 1); /* stay sane */

   // Tell the tool this thread is exiting
   VG_TRACK( pre_thread_ll_exit, tid );

   if (c == 1) {

      VG_(debugLog)(1, "syswrap-l4re", 
                       "run_a_thread_NORETURN(tid=%lld): "
                          "last one standing\n",
                          (ULong)tidW);

      /* We are the last one standing.  Keep hold of the lock and
         carry on to show final tool results, then exit the entire system. 
         Use the continuation pointer set at startup in m_main. */
      ( * VG_(address_of_m_main_shutdown_actions_NORETURN) ) (tid, src);

   } else {

      ThreadState *tst;

      VG_(debugLog)(1, "syswrap-l4re", 
                       "run_a_thread_NORETURN(tid=%lld): "
                          "not last one standing\n",
                          (ULong)tidW);

      /* OK, thread is dead, but others still exist.  Just exit. */
      tst = VG_(get_ThreadState)(tid);

      /* This releases the run lock */
      VG_(exit_thread)(tid);
      vg_assert(tst->status == VgTs_Zombie);

      /* We have to use this sequence to terminate the thread to
         prevent a subtle race.  If VG_(exit_thread)() had left the
         ThreadState as Empty, then it could have been reallocated,
         reusing the stack while we're doing these last cleanups.
         Instead, VG_(exit_thread) leaves it as Zombie to prevent
         reallocation.  We need to make sure we don't touch the stack
         between marking it Empty and exiting.  Hence the
         assembler. */
#if defined(VGO_l4re)
      //XXX: some other threads still exists, so only this thread should exit
      l4_sleep_forever();
      enter_kdebug("hmm!");
#else
# error Unknown platform
#endif

      VG_(core_panic)("Thread exit failed?\n");
   }

   /*NOTREACHED*/
   vg_assert(0);
}

Word ML_(start_thread_NORETURN) ( void* arg )
{
   ThreadState* tst = (ThreadState*)arg;
   ThreadId     tid = tst->tid;
   
   l4_utcb_tcr()->user[2] = tid;
   
   VG_(printf)("%s: hello my tid is %d\n", __func__, tid);
   
   run_a_thread_NORETURN ( (Word)tid );
   /*NOTREACHED*/
   vg_assert(0);
}

/* Allocate a stack for this thread, if it doesn't already have one.
   They're allocated lazily, and never freed.  Returns the initial stack
   pointer value to use, or 0 if allocation failed. */
Addr ML_(allocstack)(ThreadId tid)
{
   ThreadState* tst = VG_(get_ThreadState)(tid);
   VgStack*     stack;
   Addr         initial_SP;

   /* Either the stack_base and stack_init_SP are both zero (in which
      case a stack hasn't been allocated) or they are both non-zero,
      in which case it has. */

   if (tst->os_state.valgrind_stack_base == 0)
      vg_assert(tst->os_state.valgrind_stack_init_SP == 0);

   if (tst->os_state.valgrind_stack_base != 0)
      vg_assert(tst->os_state.valgrind_stack_init_SP != 0);

   /* If no stack is present, allocate one. */

   if (tst->os_state.valgrind_stack_base == 0) {
      stack = VG_(am_alloc_VgStack)( &initial_SP );
      if (stack) {
         tst->os_state.valgrind_stack_base    = (Addr)stack;
         tst->os_state.valgrind_stack_init_SP = initial_SP;
      }
   }

   if (0)
      VG_(printf)( "stack for tid %d at %p; init_SP=%p\n",
                   tid, 
                   (void*)tst->os_state.valgrind_stack_base, 
                   (void*)tst->os_state.valgrind_stack_init_SP );
                  
   return tst->os_state.valgrind_stack_init_SP;
}

/* Allocate a stack for the main thread, and run it all the way to the
   end.  Although we already have a working VgStack
   (VG_(interim_stack)) it's better to allocate a new one, so that
   overflow detection works uniformly for all threads.
*/
void VG_(main_thread_wrapper_NORETURN)(ThreadId tid)
{
   Addr sp;
   VG_(debugLog)(1, "syswr-l4", 
                    "entering VG_(main_thread_wrapper_NORETURN)\n");

   sp = ML_(allocstack)(tid);

   /* If we can't even allocate the first thread's stack, we're hosed.
      Give up. */
   vg_assert2(sp != 0, "Cannot allocate main thread's stack.");

   /* shouldn't be any other threads around yet */
   vg_assert( VG_(count_living_threads)() == 1 );

   ML_(call_on_new_stack_0_1)( 
      (Addr)sp,               /* stack */
      0,                      /* bogus return address */
      run_a_thread_NORETURN,  /* fn to call */
      (Word)tid               /* arg to give it */
   );

   /*NOTREACHED*/
   vg_assert(0);
}

/* ---------------------------------------------------------------------
   PRE/POST wrappers for arch-generic, Linux-specific syscalls
   ------------------------------------------------------------------ */

// Nb: See the comment above the generic PRE/POST wrappers in
// m_syswrap/syswrap-generic.c for notes about how they work.

#define PRE(name)       DEFN_PRE_TEMPLATE(linux, name)
#define POST(name)      DEFN_POST_TEMPLATE(linux, name)

// Combine two 32-bit values into a 64-bit value
#define LOHI64(lo,hi)   ( (lo) | ((ULong)(hi) << 32) )

#undef PRE
#undef POST

/*--------------------------------------------------------------------*/
/*--- end                                                          ---*/
/*--------------------------------------------------------------------*/



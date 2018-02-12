/*--------------------------------------------------------------------*/
/*--- Platform-specific syscalls stuff.         syswrap-x86-l4re.c ---*/
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

/* TODO/FIXME jrs 20050207: assignments to the syscall return result
   in interrupted_syscall() need to be reviewed.  They don't seem
   to assign the shadow state.
*/

#include "pub_core_basics.h"
#include "pub_core_vki.h"
#include "pub_core_vkiscnums.h"
#include "pub_core_libcsetjmp.h"
#include "pub_core_threadstate.h"
#include "pub_core_aspacemgr.h"
#include "pub_core_debuglog.h"
#include "pub_core_libcbase.h"
#include "pub_core_libcassert.h"
#include "pub_core_libcprint.h"
#include "pub_core_libcproc.h"
#include "pub_core_libcsignal.h"
#include "pub_core_mallocfree.h"
#include "pub_core_options.h"
#include "pub_core_scheduler.h"
#include "pub_core_sigframe.h"      // For VG_(sigframe_destroy)()
#include "pub_core_signals.h"
#include "pub_core_syscall.h"
#include "pub_core_syswrap.h"
#include "pub_core_tooliface.h"
#include "pub_core_stacks.h"        // VG_(register_stack)

#include "priv_types_n_macros.h"
#include "priv_syswrap-generic.h"    /* for decls of generic wrappers */
#include "priv_syswrap-linux.h"      /* for decls of linux-ish wrappers */
#include "priv_syswrap-linux-variants.h" /* decls of linux variant wrappers */
#include "priv_syswrap-main.h"

#if defined(VGO_l4re)
#include <l4/sys/types.h>
#include <l4/sys/thread.h>
#include <l4/sys/utcb.h>
#include <l4/sys/ipc.h>
#include <l4/util/util.h>
#include <l4/sys/consts.h>
#include <l4/sys/debugger.h>
#endif

/* ---------------------------------------------------------------------
   clone() handling
   ------------------------------------------------------------------ */

/* Call f(arg1), but first switch stacks, using 'stack' as the new
   stack, and use 'retaddr' as f's return-to address.  Also, clear all
   the integer registers before entering f.*/
__attribute__((noreturn))
void ML_(call_on_new_stack_0_1) ( Addr stack,
                      Addr retaddr,
                      void (*f)(Word),
                                  Word arg1 );
//  4(%esp) == stack
//  8(%esp) == retaddr
// 12(%esp) == f
// 16(%esp) == arg1
asm(
".text\n"
".globl vgModuleLocal_call_on_new_stack_0_1\n"
"vgModuleLocal_call_on_new_stack_0_1:\n"
"   movl %esp, %esi\n"     // remember old stack pointer
"   movl 4(%esi), %esp\n"  // set stack
"   pushl 16(%esi)\n"      // arg1 to stack
"   pushl  8(%esi)\n"      // retaddr to stack
"   pushl 12(%esi)\n"      // f to stack
"   movl $0, %eax\n"       // zero all GP regs
"   movl $0, %ebx\n"
"   movl $0, %ecx\n"
"   movl $0, %edx\n"
"   movl $0, %esi\n"
"   movl $0, %edi\n"
"   movl $0, %ebp\n"
"   ret\n"                 // jump to f
"   ud2\n"                 // should never get here
".previous\n"
);

// forward declarations
static void setup_child ( ThreadArchState*, ThreadArchState*, Bool, Word );
static SysRes sys_set_thread_area ( ThreadId, vki_modify_ldt_t* );

/*
   When a client creates a new thread, we need to keep track of the new thread.  This means:
   1. allocate a ThreadId+ThreadState+stack for the the thread

   2. initialize the thread's new VCPU state
      setting the initial instruction pointer to the right one

   3. create the thread using the same args as the client requested,
   but using the scheduler entrypoint for EIP, and a separate stack
   for ESP.
 */
static SysRes do_create_new_thread ( ThreadId ptid,
                                     Addr esp,
                                     l4_umword_t flags,
                                     Word client_ip)
{
   static const Bool debug = False;

   ThreadId     ctid = VG_(alloc_ThreadState)();
   ThreadState* ptst = VG_(get_ThreadState)(ptid);
   ThreadState* ctst = VG_(get_ThreadState)(ctid);
   UWord*       stack;
   UWord        vg_stack;
   NSegment const* seg;
   SysRes       res;
   Int          eax;
   UWord         vg_eip;
   char         *thread_name = VG_(malloc)("", 15); /* VG::threadXYZ */
   vg_assert(VG_(is_running_thread)(ptid));
   vg_assert(VG_(is_valid_tid)(ctid));

   stack = (UWord*)ML_(allocstack)(ctid);
   if (stack == NULL) {
      res = VG_(mk_SysRes_Error)( VKI_ENOMEM );
      goto out;
   }

   /* Copy register state

      Both parent and child return to the same place, and the code
      following the clone syscall works out which is which, so we
      don't need to worry about it.

      The parent gets the child's new tid returned from clone, but the
      child gets 0.

      If the clone call specifies a NULL esp for the new thread, then
      it actually gets a copy of the parent's esp.
   */
   /* Note: the clone call done by the Quadrics Elan3 driver specifies
      clone flags of 0xF00, and it seems to rely on the assumption
      that the child inherits a copy of the parent's GDT.
      setup_child takes care of setting that up. */
   setup_child( &ctst->arch, &ptst->arch, True, client_ip );

   /* Make sys_clone appear to have returned Success(0) in the
      child. */

   // TODO ctst->arch.vex.guest_EAX = 0;

   if (esp != 0)
      ctst->arch.vex.guest_ESP = esp;

   ctst->os_state.parent = ptid;

   /* We don't really know where the client stack is, because its
      allocated by the client.  The best we can do is look at the
      memory mappings and try to derive some useful information.  We
      assume that esp starts near its highest possible value, and can
      only go down to the start of the mmaped segment. */
   seg = VG_(am_find_nsegment)((Addr)esp);
   if (seg && seg->kind != SkResvn) {
      ctst->client_stack_highest_word = (Addr)VG_PGROUNDUP(esp);
      ctst->client_stack_szB = ctst->client_stack_highest_word - seg->start;

      VG_(register_stack)(seg->start, ctst->client_stack_highest_word);

      if (debug)
     VG_(printf)("tid %d: guessed client stack range %#lx-%#lx\n",
             ctid, seg->start, VG_PGROUNDUP(esp));
   } else {
      VG_(message)(Vg_UserMsg, "!? New thread %d starts with ESP(%#lx) unmapped\n",
           ctid, esp);
      ctst->client_stack_szB  = 0;
   }

   /* Assume the clone will succeed, and tell any tool that wants to
      know that this thread has come into existence.  We cannot defer
      it beyond this point because sys_set_thread_area, just below,
      causes tCheck to assert by making references to the new ThreadId
      if we don't state the new thread exists prior to that point.
      If the clone fails, we'll send out a ll_exit notification for it
      at the out: label below, to clean up. */
   VG_TRACK ( pre_thread_ll_create, ptid, ctid );

   /* Create the new thread */
/*   eax = do_syscall_clone_x86_linux(
            ML_(start_thread_NORETURN), stack, flags, &VG_(threads)[ctid],
            child_tidptr, parent_tidptr, NULL
         );*/
/*   l4_thread_ex_regs_u( ptst->arch.vex.guest_EDX, // cap
                        ML_(start_thread_NORETURN), // ip
                        stack, //esp
                        0, //flags
                        l4_utcb_wrap());*/

   /* Therefore Valgrind doesn't lose control over the client thread
    * a special function is called in valgrinds context */
   vg_eip = (l4_umword_t) &ML_(start_thread_NORETURN);

   *stack =  (UWord) &VG_(threads)[ctid];
   stack--;

   vg_stack = (l4_umword_t) stack;

   VG_(debugLog)(0, "syswrap", "creating new thread\n"
                               "\t\t\t  (vg) ip    = 0x%08lx\n"
                               "\t\t\t  (vg) stack = 0x%08x\n"
                               "\t\t\t  (cl) ip    = 0x%08x\n"
                               "\t\t\t  (cl) stack = 0x%08x\n"
                               "\t\t\t       flags = 0x%08x\n",
                               vg_eip,
                               (unsigned int)stack,
                               ctst->arch.vex.guest_EIP,
                               ctst->arch.vex.guest_ESP,
                               flags);

   l4_thread_ex_regs_ret_u( ptst->arch.vex.guest_EDX, /* capability */
                            &vg_eip,                     /* instruction pointer (vg context) */
                            &vg_stack,                /* stack pointer (vg context) */
                            (l4_umword_t *) &flags,   /* flags */
                            l4_utcb_wrap());

   /* Every thread in Valgrind gets a name - a nice feature
    * of the fiasco micro kernel, good for debugging */

   if (VG_(snprintf)(thread_name, 15, "VG::thread%d", ctid) > 0)
     l4_debugger_set_object_name(ptst->arch.vex.guest_EDX, thread_name);

   eax = 1;
   res = VG_(mk_SysRes_x86_l4re)( eax );
  out:
   if (sr_isError(res)) {
      /* clone failed */
      VG_(cleanup_thread)(&ctst->arch);
      ctst->status = VgTs_Empty;
      /* oops.  Better tell the tool the thread exited in a hurry :-) */
      VG_TRACK( pre_thread_ll_exit, ctid );
   }

   return res;
}

/* ---------------------------------------------------------------------
   More thread stuff
   ------------------------------------------------------------------ */

void VG_(cleanup_thread) ( ThreadArchState* arch )
{
   /* Release arch-specific resources held by this thread. */
}


static void setup_child ( /*OUT*/ ThreadArchState *child,
                          /*IN*/  ThreadArchState *parent,
                          Bool inherit_parents_GDT,
                          Word client_ip)
{
   /* We inherit our parent's guest state. */
   child->vex = parent->vex;
   child->vex_shadow1 = parent->vex_shadow1;
   child->vex_shadow2 = parent->vex_shadow2;
   /* In L4Re creating a new thread means not cloning like in linux,
    * instead a new instruction pointer is required */
   child->vex.guest_EIP = client_ip;
}


/* ---------------------------------------------------------------------
   PRE/POST wrappers for x86/Linux-specific syscalls
   ------------------------------------------------------------------ */

#define PRE(name)       DEFN_PRE_TEMPLATE(x86_l4re, name)
#define POST(name)      DEFN_POST_TEMPLATE(x86_l4re, name)

#define FOO(name)       case L4_PROTO_##name : VG_(printf)(#name); break;
enum
{
    L4_PROTO_DATASPACE = 0x4000,
    L4_PROTO_NAMESPACE,
    L4_PROTO_PARENT,
    L4_PROTO_GOOS,
    L4_PROTO_MEMALLOC,
    L4_PROTO_RM,
    L4_PROTO_EVENT,
};
/*
 * print some informations about current syscall
 * see also l4sys/include/err.h
 *          l4sys/include/types.h
 */

void print_infos_to_syscall(l4_msgtag_t *tag, ThreadId tid) {
    VG_(printf)("msgtag_label = %lx\n", l4_msgtag_label(*tag));
    {
        VG_(printf)("  Protocol: ");
        switch(l4_msgtag_label(*tag)) {
            FOO(NONE);
            case L4_PROTO_ALLOW_SYSCALL: // =L4_PROTO_PF_EXCEPTION
            VG_(printf)("ALLOW_SYSCALL/PF_EXCEPTION");
            FOO(IRQ);
            FOO(PAGE_FAULT);
            FOO(PREEMPTION);
            FOO(SYS_EXCEPTION);
            FOO(EXCEPTION);
            FOO(SIGMA0);
            FOO(IO_PAGE_FAULT);
            FOO(FACTORY);
            FOO(TASK);
            FOO(THREAD);
            FOO(LOG);
            FOO(SCHEDULER);
            FOO(DATASPACE);
            FOO(NAMESPACE);
            FOO(PARENT);
            FOO(GOOS);
            FOO(RM);
            FOO(MEMALLOC);
            FOO(EVENT);
            default:               VG_(printf)("unknown");
                                   break;
        }
        VG_(printf)("\n");
    }
}

#undef FOO

/*
 * Determine if system call arguments indicate task termination.
 */
Bool syscall_is_exit(SyscallArgs* args)
{
    return (args->arg4 /* EDX */ == (L4_INVALID_CAP | L4_SYSF_RECV))
        && (args->arg3 /* ECX */ == L4_IPC_NEVER.raw );
}

/*
 * Determine if syscall arguments are for thread_ex_regs
 */
Bool syscall_is_threadexregs(SyscallArgs *args, l4_msg_regs_t *mr, l4_msgtag_t tag)
{
    return (l4_msgtag_label(tag) == L4_PROTO_THREAD)
        && (mr->mr[0] & L4_THREAD_EX_REGS_OP);
}


/*
 * Determine if system call would perform a GDT set.
 */
Bool syscall_is_setgdt(l4_msg_regs_t *mr, l4_msgtag_t tag)
{
    return (l4_msgtag_label(tag) == L4_PROTO_THREAD)
        && (mr->mr[0] == L4_THREAD_X86_GDT_OP);
}


/*
 * Determine if syscall is a PARENT signal IPC.
 */
Bool syscall_is_parentsignal(l4_msg_regs_t *mr, l4_msgtag_t tag)
{
   return (l4_msgtag_label(tag) == L4_PROTO_PARENT)
       && (mr->mr[1] == 0);
}


/*
 * Handle GDT set system call.
 *
 * This code basically does what Fiasco does, but uses the guest_GDT
 * that was allocated upon thread creation instead of performing a
 * system call.
 */
int handle_gdt_set(l4_msg_regs_t *mr, l4_msgtag_t tag, ThreadState *ts)
{
    enum { GDT_USER_ENTRY1 = 0x48 };
    unsigned idx        = mr->mr[1];
    unsigned words      = l4_msgtag_words(tag);
    Addr gdt_user_start = (Addr)(ts->arch.vex.guest_GDT + GDT_USER_ENTRY1);

    if (words == 0) {
        return 1; // invalid
    }
    if (words == 1) { // case 1: read entry
        mr->mr[0] = *(unsigned int *)gdt_user_start;
    } else {
        words -= 2; // -2 words are opcode and index

        VG_(memcpy)(gdt_user_start + 2 * idx,
                    &mr->mr[2], words * sizeof(unsigned));

        /*
         * Return value is a msgtag with the label set to the proper GDT entry.
         * This needs to be correct as it will later on be used to index the GDT.
         */
        ts->arch.vex.guest_EAX = ((idx << 3) + GDT_USER_ENTRY1 + 3) << 16;
    }

    return 0;
}


PRE(generic)
{
#define DEBUG_MYSELF 0
    ThreadState* tst;
    l4_utcb_t *u;
    l4_msg_regs_t *v;
    l4_msgtag_t *tag;
    if (0) VG_(printf)("PRE_generic: sysno = %08lx arg0 = %8lx arg1 = %8lx arg2 = %8lx\n"
                       "             arg3  = %8lx arg4 = %8lx arg5 = %8lx arg6 = %8lx\n",
                       arrghs->sysno, arrghs->arg1, arrghs->arg2, arrghs->arg3, arrghs->arg4,
                       arrghs->arg5, arrghs->arg6, arrghs->arg7);
    /* get access to virtual utcb of client */
    u = ts_utcb(&VG_(threads)[tid]);
    v = l4_utcb_mr_u(u);

    tag = (l4_msgtag_t *) &(arrghs->arg1);
    tst = VG_(get_ThreadState)(tid);

#if DEBUG_MYSELF
    print_infos_to_syscall(tag, tid);
    VG_(get_and_pp_StackTrace)( tid, VG_(clo_backtrace_size) );
#endif
    if (syscall_is_exit(arrghs)) {
        /* l4_sleep_forever */
        tst->exitreason = VgSrc_ExitThread;
        tst->os_state.exitcode = 1;
        SET_STATUS_Success(0);
        //enter_kdebug("l4_sleep_forever");

    } else if (syscall_is_threadexregs(arrghs, v, *tag)) {
        // TODO is this really a thread-create??
        // TODO store thread cap
        // catch cap mappings

        /* the guest wants to create a new thread */
#if DEBUG_MYSELF
        VG_(debugLog)(1, "syswrap", "The client wants to create a new thread\n"
                                    "\t\t\tcollected infos (from virt utcb):\n"
                                    "\t\t\t  (cl) ip = 0x%x\n"
                                    "\t\t\t  (cl) sp = 0x%x\n"
                                    "\t\t\t    flags = 0x%x\n",
                                    (unsigned int) v->mr[1],
                                    (unsigned int) v->mr[2],
                                    (unsigned int) v->mr[0]);
#endif
        if (0) enter_kdebug("before thread create");

        do_create_new_thread ( tid,
                               v->mr[2], /*stack pointer*/
                               v->mr[0], /*flags*/
                               v->mr[1]  /*instruction pointer*/
                             );

        if (0) enter_kdebug("after thread create");
        SET_STATUS_Success(0);
        *flags |= SfYieldAfter;

    } else if (syscall_is_setgdt(v, *tag)) {
        SET_STATUS_Success(handle_gdt_set(v, *tag, tst));

    } else if (syscall_is_parentsignal(v, *tag)) {
        /* the guest signals his parent that he would exit now */
#if DEBUG_MYSELF
        VG_(debugLog)(0, "syswrap", "The client would like to exit\n");
        VG_(debugLog)(0, "syswrap", "exit code = 0x%x\n", (unsigned int) v->mr[2]);
#endif
        tst->exitreason = VgSrc_ExitThread;
        tst->os_state.exitcode = v->mr[2];

        /* For a correct exit we must "kill" all threads, but i don't
         * know if this is the right solution.
         */
        {
            ThreadId _tid;
            ThreadState *_tst;
            for (_tid = 1; _tid < VG_N_THREADS; _tid++) {
                if ( VG_(threads)[_tid].status != VgTs_Empty &&
                     VG_(threads)[_tid].status != VgTs_Zombie &&
                     _tid != tid) {
                    _tst =  VG_(get_ThreadState)(_tid);
                    VG_(threads)[_tid].status = VgTs_Zombie;
                    tst->exitreason = VgSrc_ExitThread;
                }
            }
        }
        SET_STATUS_Success(0);

    } else {
        /*
         * In all other cases this might be a "normal" IPC and thus might block.
         */
        *flags |= SfMayBlock;
    }
#undef DEBUG_MYSELF
}

POST(generic)
{
  if (0) VG_(printf)("\
      POST_generic: sysno %8lx arg0 = %8lx arg1 = %8lx arg2 = %8lx\n"
     "              arg3 =  %8lx arg4 = %8lx arg5 = %8lx arg6 = %8lx\n",
      arrghs->sysno, arrghs->arg1, arrghs->arg2,
      arrghs->arg3, arrghs->arg4, arrghs->arg5, arrghs->arg6, arrghs->arg7);
}


PRE(dummy) { }
POST(dummy) { }

#undef PRE
#undef POST


/* ---------------------------------------------------------------------
   The x86/l4re syscall table
   ------------------------------------------------------------------ */

/* Add an x86-l4re specific wrapper to a syscall table. */
#define PLAX_(sysno, name)    WRAPPER_ENTRY_X_(x86_l4re, sysno, name)
#define PLAXY(sysno, name)    WRAPPER_ENTRY_XY(x86_l4re, sysno, name)


// This table maps from __NR_xxx syscall numbers (from
// linux/include/asm-i386/unistd.h) to the appropriate PRE/POST sys_foo()
// wrappers on x86 (as per sys_call_table in linux/arch/i386/kernel/entry.S).
//
// For those syscalls not handled by Valgrind, the annotation indicate its
// arch/OS combination, eg. */* (generic), */Linux (Linux only), ?/?
// (unknown).

const SyscallTableEntry ML_(syscall_table)[] = {
  PLAXY(SYS_INVOKE, generic),
  PLAXY(SYS_DEBUG, dummy),
  PLAXY(SYS_ENTER_KDEBUG, dummy),
  PLAXY(SYS_LINUX_INT80, dummy),
  PLAXY(SYS_UD2, dummy),
  PLAXY(SYS_ARTIFICIAL, dummy),
};

const UInt ML_(syscall_table_size) =
            sizeof(ML_(syscall_table)) / sizeof(ML_(syscall_table)[0]);

/*--------------------------------------------------------------------*/
/*--- end                                                          ---*/
/*--------------------------------------------------------------------*/

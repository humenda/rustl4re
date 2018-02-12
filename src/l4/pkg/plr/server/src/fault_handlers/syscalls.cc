/*
 * syscalls.cc --
 *
 *     Implementation of Romain syscall handling.
 *
 * (c) 2011-2013 Björn Döbel <doebel@os.inf.tu-dresden.de>,
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include "../log"
#include "../app_loading"
#include "../locking.h"
#include "../manager"
#include "../thread_group.h"

#include "observers.h"

#include <l4/re/rm-sys.h>
#include <l4/re/util/region_mapping_svr>
#include <l4/sys/segment.h>
#include <l4/sys/consts.h>
#include <l4/cxx/ipc_stream>

#define MSG() DEBUGf(Romain::Log::Faults)
#define MSGt(t) DEBUGf(Romain::Log::Faults) << "[" << t->vcpu() << "] "
#define DEBUGt(t) DEBUG() <<  "[" << t->vcpu() << "] "

#include "syscalls_factory.h" // uses MSG() defined above (ugly)

DEFINE_EMPTY_STARTUP(SyscallObserver)

static l4_umword_t num_syscalls;


void Romain::SyscallObserver::status() const
{
	INFO() << "[sys ] System call count: " << num_syscalls;
}


static Romain::ThreadHandler          threadsyscall;
static Romain::SyscallHandler         nullhandler;
static Romain::RegionManagingHandler  rm;
static Romain::Scheduling             sched;
static Romain::IrqHandler             irq;
static Romain::IrqSenderHandler       irq_sender;

Romain::Factory theObjectFactory;

Romain::Observer::ObserverReturnVal
Romain::SyscallObserver::notify(Romain::App_instance *i,
                                Romain::App_thread *t,
                                Romain::Thread_group *tg,
                                Romain::App_model *a)
{
	enum { Syscall_magic_address = 0xEACFF003, };
	Romain::Observer::ObserverReturnVal retval = Romain::Observer::Ignored;

	if (t->vcpu()->r()->trapno != 13) {
		return retval;
	}

	/* SYSENTER / INT30 */
	if (t->vcpu()->r()->ip == Syscall_magic_address) {
#if BENCHMARKING
	unsigned long long t1, t2;
	t1 = l4_rdtsc();
#endif
		++num_syscalls;

		l4_msgtag_t *tag = reinterpret_cast<l4_msgtag_t*>(&t->vcpu()->r()->ax);
		MSG() << "SYSENTER(" << tg->name << ") tag = " << std::hex << tag->label();

#if EVENT_LOGGING
		Measurements::GenericEvent* ev = Romain::globalLogBuf->next();
		ev->header.tsc     = Romain::globalLogBuf->getTime(Log::logLocalTSC);
		ev->header.vcpu    = (l4_uint32_t)t->vcpu();
		ev->header.type    = Measurements::Syscall;
		ev->data.sys.eip   = t->vcpu()->r()->bx;
		ev->data.sys.label = tag->label();
#endif

		/*
		 * Fiasco-specific:
		 *      EBX is return address
		 *      EBP is return ESP
		 * -> we need to remember these
		 */
		l4_addr_t ebx_pre = t->vcpu()->r()->bx;
		l4_addr_t ebp_pre = t->vcpu()->r()->bp;

		switch(tag->label()) {

			case L4_PROTO_NONE:
				/*
				 * Catch the open_wait versions of IPC as they require
				 * redirection through the gateagent thread. Open wait
				 * is defined by the IPC bindings as one where EDX is set
				 * to L4_INVALID_CAP | <some_flags>.
				 */
				if ((t->vcpu()->r()->dx & L4_INVALID_CAP) == L4_INVALID_CAP) {
					tg->gateagent->trigger_agent(t);
				} else {
					nullhandler.proxy_syscall(i, t, tg, a);
				}
				retval = Romain::Observer::Replicatable;
				break;

			case L4_PROTO_FACTORY:
				retval = theObjectFactory.handle(i, t, tg, a);
				break;

			case L4_PROTO_IRQ:
				retval = irq.handle(i, t, tg, a);
				break;

			case L4_PROTO_IRQ_SENDER:
				retval = irq_sender.handle(i, t, tg, a);
				break;

			case L4_PROTO_THREAD:
				/* 
				 * Each instance needs to perform its own
				 * thread creation.
				 */
				retval = threadsyscall.handle(i, t, tg, a);
				break;

			case L4_PROTO_TASK:
				if ((t->vcpu()->r()->dx & ~0xF) == L4RE_THIS_TASK_CAP) {
					retval = handle_task(i, t, a);
				} else {
					nullhandler.proxy_syscall(i, t, tg, a);
					retval = Romain::Observer::Replicatable;
				}
				break;

                        case L4Re::Rm::Protocol:
				/*
				 * Region management is done only once as e.g.,
				 * regions for attaching need to be replicated
				 * across instances. The real adaptation then
				 * happens during page fault handling.
				 */
				retval = rm.handle(i, t, tg, a);
				break;

                        case L4Re::Parent::Protocol:
				/*
				 * The parent protocol is only used for exitting.
				 */
				{
					struct timeval tv;
					gettimeofday(&tv, 0);
					INFO() << "Instance " << i->id() << " exitting. Time "
					       << YELLOW << tv.tv_sec << "." << tv.tv_usec
					       << NOCOLOR;

#if EVENT_LOGGING
					Measurements::GenericEvent* ev = Romain::globalLogBuf->next();
					ev->header.tsc                 = Romain::globalLogBuf->getTime(Log::logLocalTSC);
					ev->header.vcpu                = (l4_uint32_t)t->vcpu();
					ev->header.type                = Measurements::Thread_stop;
#endif

					Romain::_the_instance_manager->show_stats();
					
					if (REBOOT_ON_EXIT) enter_kdebug("*#^");
					else exit(0);

					nullhandler.proxy_syscall(i, t, tg, a);
					retval = Romain::Observer::Replicatable;
				}
				break;

			case L4_PROTO_SCHEDULER:
				retval = sched.handle(i, t, tg, a);
				break;

			default:
				/*
				 * Proxied syscalls are always only executed
				 * once because for the outside world it must
				 * look as if the master server _is_ the one
				 * application everyone is talking to.
				 */
				nullhandler.proxy_syscall(i, t, tg, a);
				retval = Romain::Observer::Replicatable;
				break;
		
		}

		t->vcpu()->r()->ip = ebx_pre;
		t->vcpu()->r()->sp = ebp_pre;

#if BENCHMARKING
	t2 = l4_rdtsc();
	t->count_syscalls(t2-t1);
#endif
	} else if (t->vcpu()->r()->err == 0x192) {
		// INT 0x32 - debug syste call. We currently ignore it.
		t->vcpu()->r()->ip += 2;
		retval = Romain::Observer::Replicatable;
	} else if (t->vcpu()->r()->err == 0x148) { // INT $41 -- NOOP syscall
		t->vcpu()->r()->ip += 2;
		Romain::Log::logFlags = Romain::Log::All;
		retval = Romain::Observer::Replicatable;
	} else if (t->vcpu()->r()->err == 0x152) { // INT $42 -- enable logging
		INFO() << "[" << std::hex << (l4_umword_t)t->vcpu() <<  "] INT 42 ("
			   << t->vcpu()->r()->ip << ")";
		t->vcpu()->print_state();
		t->vcpu()->r()->ip += 2;
		Romain::Log::logFlags = Romain::Log::All;
		retval = Romain::Observer::Replicatable;
	} else {
		t->vcpu()->print_state();
		INFO() << "err = " << std::hex << t->vcpu()->r()->err;
	}

	return retval;
}


void Romain::SyscallHandler::proxy_syscall(Romain::App_instance *,
                                           Romain::App_thread* t,
                                           Romain::Thread_group* tg,
                                           Romain::App_model *)
{
	char backup_utcb[L4_UTCB_OFFSET]; // for storing local UTCB content

	l4_utcb_t *addr = reinterpret_cast<l4_utcb_t*>(t->remote_utcb());
	l4_utcb_t *cur_utcb = l4_utcb();
	MSGt(t) << "UTCB @ " << std::hex << (l4_umword_t)addr;
	_check((l4_addr_t)addr == ~0UL, "remote utcb ptr??");

	/*
	 * We are going to perform the system call on behalf of the client. This will
	 * thrash our local UTCB, so we want to store it here.
	 */
	store_utcb((char*)cur_utcb, backup_utcb);
	store_utcb((char*)addr, (char*)cur_utcb);

	//t->print_vcpu_state();
	//Romain::dump_mem((l4_umword_t*)addr, 40);

	/* Perform Fiasco system call */
	asm volatile (L4_ENTER_KERNEL
	              : "=a" (t->vcpu()->r()->ax),
	              "=b" (t->vcpu()->r()->bx),
	              /* ECX, EDX are overwritten anyway */
	              "=S" (t->vcpu()->r()->si),
	              "=D" (t->vcpu()->r()->di)
	              : "a" (t->vcpu()->r()->ax),
	              /* EBX and EBP will be overwritten with local
	               * values in L4_ENTER_KERNEL */
	                "c" (t->vcpu()->r()->cx),
	                "d" (t->vcpu()->r()->dx),
	                "S" (t->vcpu()->r()->si),
	                "D" (t->vcpu()->r()->di)
                        L4S_PIC_SYSCALL
	              : "memory", "cc"
	);

	/*
	 * Restore my UTCB
	 */
	store_utcb((char*)cur_utcb, (char*)addr);
	store_utcb(backup_utcb, (char*)cur_utcb);

	//t->print_vcpu_state();
	//Romain::dump_mem((l4_umword_t*)addr, 40);
}


Romain::Observer::ObserverReturnVal
Romain::RegionManagingHandler::handle(Romain::App_instance* i,
                                      Romain::App_thread* t,
                                      Romain::Thread_group* tg,
                                      Romain::App_model * a)
{
	MSGt(t) << "RM PROTOCOL";

	l4_utcb_t *utcb = reinterpret_cast<l4_utcb_t*>(t->remote_utcb());
	MSGt(t) << "UTCB @ " << std::hex << (l4_umword_t)utcb;
	_check((l4_addr_t)utcb == ~0UL, "remote utcb ptr??");

	//t->print_vcpu_state();
	//Romain::dump_mem((l4_umword_t*)utcb, 40, 4);

	{
		Romain::Rm_guard r(a->rm(), i->id());
		L4::Ipc::Iostream ios(utcb);
		L4Re::Util::region_map_server<Romain::Region_map_server>((void*)0, a->rm(), ios);
		t->vcpu()->r()->ax = 0;
	}

	//t->print_vcpu_state();
	//Romain::dump_mem((l4_umword_t*)utcb, 40);

	return Romain::Observer::Replicatable;
}


Romain::Observer::ObserverReturnVal
Romain::ThreadHandler::handle(Romain::App_instance *i,
                              Romain::App_thread* t,
                              Romain::Thread_group * tg,
                              Romain::App_model *am)
{
	MSGt(t) << "Thread system call";
	l4_utcb_t *utcb = reinterpret_cast<l4_utcb_t*>(t->remote_utcb());
	MSGt(t) << "UTCB @ " << std::hex << (l4_umword_t)utcb;
	_check((l4_addr_t)utcb == ~0UL, "remote utcb ptr??");

	//t->print_vcpu_state();
	//Romain::dump_mem((l4_umword_t*)utcb, 40);

	l4_umword_t op   = l4_utcb_mr_u(utcb)->mr[0] & L4_THREAD_OPCODE_MASK;
	l4_umword_t dest = t->vcpu()->r()->dx & L4_CAP_MASK;

	DEBUG() << "dest cap " << std::hex << dest << " " << L4_INVALID_CAP;
	Romain::Thread_group* group = (Romain::Thread_group*)0xdeadbeef;
	if (dest == L4_INVALID_CAP) {
		group = tg;
	} else {
		group = theObjectFactory.thread_for_cap(dest >> L4_CAP_SHIFT);
	}
	DEBUG() << "tgroup " << group;

	switch(op) {
		case L4_THREAD_CONTROL_OP:
			group->control(t, utcb, am);
			return Romain::Observer::Replicatable;
		case L4_THREAD_EX_REGS_OP:
			group->ex_regs(t);
			break;
		case L4_THREAD_SWITCH_OP:
			ERROR() << "THREAD: switch\n";
			break;
		case L4_THREAD_STATS_OP:
			ERROR() << "THREAD: stats\n";
			break;
		case L4_THREAD_VCPU_RESUME_OP:
			ERROR() << "THREAD: vcpu_resume\n";
			break;
		case L4_THREAD_REGISTER_DELETE_IRQ_OP:
			ERROR() << "THREAD: irq\n";
			break;
		case L4_THREAD_MODIFY_SENDER_OP:
			ERROR() << "THREAD: modify sender\n";
			break;
		case L4_THREAD_VCPU_CONTROL_OP:
			ERROR() <<"THREAD: vcpu control\n";
			break;
		case L4_THREAD_VCPU_CONTROL_EXT_OP:
			ERROR() << "THREAD: vcpu control ext\n";
			break;
		case L4_THREAD_X86_GDT_OP:
			group->gdt(t, utcb);
			break;
		default:
			ERROR() << "unknown thread op: " << std::hex << op << "\n";
			break;
	}

	return Romain::Observer::Replicatable;
}


Romain::Observer::ObserverReturnVal
Romain::SyscallObserver::handle_task(Romain::App_instance* i,
                                          Romain::App_thread*   t,
                                          Romain::App_model*    a)
{
	l4_utcb_t   *utcb = reinterpret_cast<l4_utcb_t*>(t->remote_utcb());
	l4_umword_t    op = l4_utcb_mr_u(utcb)->mr[0] & L4_THREAD_OPCODE_MASK;
	Romain::Observer::ObserverReturnVal ret = Romain::Observer::Finished;

	switch(op) {
		case L4_TASK_UNMAP_OP:
			{
				l4_fpage_t fp;
				fp.raw = l4_utcb_mr_u(utcb)->mr[2];
				MSGt(t) << "Task::unmap(p = " << std::hex
				        << l4_fpage_page(fp) << ", sz = " << l4_fpage_size(fp) << ")";
				i->vcpu_task()->unmap(fp, L4_FP_ALL_SPACES, utcb);
			}
			//t->vcpu()->r()->ax = 0;
			break;
		case L4_TASK_CAP_INFO_OP:
			nullhandler.proxy_syscall(i,t,0,a);
			break;
		default:
			MSGt(t) << "Task system call";
			MSGt(t) << "UTCB @ " << std::hex << (l4_umword_t)utcb << " op: " << op
				  << " cap " << (t->vcpu()->r()->dx & ~0xF) << " " << L4RE_THIS_TASK_CAP;
			t->print_vcpu_state();
			break;
	}

	return ret;
}


Romain::Observer::ObserverReturnVal
Romain::Factory::handle(Romain::App_instance* inst,
                        Romain::App_thread* t,
                        Romain::Thread_group* tg,
                        Romain::App_model* am)
{
	MSGt(t) << "Factory system call";
	l4_utcb_t *utcb = reinterpret_cast<l4_utcb_t*>(t->remote_utcb());
	MSGt(t) << "UTCB @ " << std::hex << (l4_umword_t)utcb;
	_check((l4_addr_t)utcb == ~0UL, "remote utcb ptr??");

	l4_mword_t obj = l4_utcb_mr_u(utcb)->mr[0];
	l4_umword_t cap = l4_utcb_br_u(utcb)->br[0] & ~L4_RCV_ITEM_SINGLE_CAP;
	MSGt(t) << std::hex << L4_PROTO_THREAD;
	MSGt(t) << "object type: " << std::hex << obj
	      << " cap: " << cap;

	switch(obj) {
		case L4_PROTO_THREAD:
			create_thread(inst, t, tg, am, cap);
			return Romain::Observer::Replicatable;

		case L4_PROTO_IRQ:
			create_irq(inst, t, tg, am, cap);
			return Romain::Observer::Replicatable;

                case L4Re::Dataspace::Protocol:
			{
			SyscallHandler::proxy_syscall(inst, t, tg, am);
			return Romain::Observer::Replicatable;
			}

		default:
			break;
	}

	return Romain::Observer::Finished;
}


Romain::Observer::ObserverReturnVal
Romain::Scheduling::handle(Romain::App_instance* inst,
                           Romain::App_thread* t,
                           Romain::Thread_group* tg,
                           Romain::App_model* am)
{
	l4_utcb_t *utcb = reinterpret_cast<l4_utcb_t*>(t->remote_utcb());
	l4_umword_t op  = l4_utcb_mr_u(utcb)->mr[0];

	MSGt(t) << "\033[32mschedule(" << std::hex << op << ")\033[0m";
	if (op == L4_SCHEDULER_RUN_THREAD_OP) {
		l4_umword_t cap             = l4_utcb_mr_u(utcb)->mr[6] >> L4_CAP_SHIFT;
		Romain::Thread_group* group = theObjectFactory.thread_for_cap(cap);
		group->scheduler_run(t);
	} else {
		SyscallHandler::proxy_syscall(inst, t, tg, am);
	}

	return Romain::Observer::Replicatable;
}


Romain::Observer::ObserverReturnVal
Romain::IrqHandler::handle(Romain::App_instance* inst,
                           Romain::App_thread* t,
                           Romain::Thread_group* tg,
                           Romain::App_model* am)
{
	l4_utcb_t *utcb = reinterpret_cast<l4_utcb_t*>(t->remote_utcb());
	l4_umword_t op     = l4_utcb_mr_u(utcb)->mr[0];
	l4_umword_t label  = l4_utcb_mr_u(utcb)->mr[1];
	l4_umword_t cap    = t->vcpu()->r()->dx & L4_CAP_MASK;

	L4::Cap<L4::Irq> irq(cap);

	DEBUGt(t) << "IRQ: cap = " << std::hex << cap << " op = " << op; 

	if (!theObjectFactory.is_irq(cap)) {
		SyscallHandler::proxy_syscall(inst, t, tg, am);
		return Romain::Observer::Replicatable;
	}

	switch(op) {
		case L4_IRQ_OP_TRIGGER:
			DEBUGt(t) << ":: trigger";
			irq->trigger();
			break;
		case L4_IRQ_OP_EOI:
			DEBUGt(t) << ":: eoi";
			tg->gateagent->trigger_agent(t);
			break;
	}

	return Romain::Observer::Replicatable;
}
Romain::Observer::ObserverReturnVal
Romain::IrqSenderHandler::handle(Romain::App_instance* inst,
                                 Romain::App_thread* t,
                                 Romain::Thread_group* tg,
                                 Romain::App_model* am)
{
	l4_utcb_t *utcb = reinterpret_cast<l4_utcb_t*>(t->remote_utcb());
	l4_umword_t op     = l4_utcb_mr_u(utcb)->mr[0];
	l4_umword_t label  = l4_utcb_mr_u(utcb)->mr[1];
	l4_umword_t cap    = t->vcpu()->r()->dx & L4_CAP_MASK;

	L4::Cap<L4::Irq> irq(cap);

	DEBUGt(t) << "IRQ: cap = " << std::hex << cap << " op = " << op; 

	if (!theObjectFactory.is_irq(cap)) {
		SyscallHandler::proxy_syscall(inst, t, tg, am);
		return Romain::Observer::Replicatable;
	}

	switch(op) {
		/*
		 * For attach(), we cannot simply redirect to the gate
		 * agent, because we need to modify the thread that is
		 * attached to the IRQ
		 */
		case L4_IRQ_SENDER_OP_ATTACH:
			{
				l4_umword_t attach_cap      = l4_utcb_mr_u(utcb)->mr[3] & L4_FPAGE_ADDR_MASK;
				DEBUG() << "attach " << std::hex << (attach_cap >> L4_CAP_SHIFT);
				Romain::Thread_group *group = theObjectFactory.thread_for_cap(attach_cap >> L4_CAP_SHIFT);
				l4_msgtag_t ret;

				if (!group) {
					ERROR() << "Unimplemented: Attaching someone else but myself!\n";
				}

				ret = irq->attach(label, group->gateagent->listener_cap);

				t->vcpu()->r()->ax = ret.raw;
				DEBUG() << std::hex << ret.raw ;
				return Romain::Observer::Replicatable;
			}
			break;
	}

	return Romain::Observer::Replicatable;
}

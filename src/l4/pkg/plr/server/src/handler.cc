/*
 * Exception handling
 *
 * Here's where the real stuff is going on
 *
 * (c) 2011-2013 Björn Döbel <doebel@os.inf.tu-dresden.de>,
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include "manager"
#include "log"
#include "exceptions"
#include "emulation"
#include "app_loading"
#include "fault_handlers/syscalls_handler.h"

#include <cassert>

#include <l4/sys/kdebug.h>
#include <l4/util/bitops.h>
#include <l4/util/rdtsc.h>

#include <pthread-l4.h>
#include <l4/sys/segment.h>

#define MSG() DEBUGf(Romain::Log::Faults)
#define MSGi(inst) MSG() << "[" << (inst)->id() << "] "
#define MSGit(inst,tg) MSG() << "[" << (inst)->id() << "] " << PURPLE << "{" << tg->name << "}" << NOCOLOR

EXTERN_C void *pthread_fn(void *data);
EXTERN_C void *pthread_fn(void *data)
{
	Romain::App_thread *t = (Romain::App_thread*)data;

	t->vcpu_cap(Pthread::L4::cap(pthread_self()));

	DEBUG() << "vcpu @ " << (void*)t->vcpu();
	DEBUG() << "thread entry: " << (void*)t->thread_entry();

	/*
	 * Thread creation, copied from the example again.
	 */
	L4::Thread::Attr attr;
	attr.pager(L4::cap_reinterpret_cast<L4::Thread>(L4Re::Env::env()->rm()));
	attr.exc_handler(L4Re::Env::env()->main_thread());
//	attr.bind(t->vcpu_utcb(), L4Re::This_task);

	chksys(t->vcpu_cap()->control(attr), "control");
	chksys(t->vcpu_cap()->vcpu_control((l4_addr_t)t->vcpu()), "enable VCPU");

	l4_sched_param_t sp = l4_sched_param(2);
	sp.affinity = l4_sched_cpu_set(t->cpu(), 0);
	chksys(L4Re::Env::env()->scheduler()->run_thread(t->vcpu_cap(),
	                                                 sp));

#if 0
	MSG() << "!" << std::hex << (void*)t->thread_sp();
	MSG() << "?" << std::hex << (void*)t->handler_sp();
#endif
	asm volatile("mov %0, %%esp\n\t"
				 "jmp *%1\n\t"
				 :
				 : "r" (t->handler_sp()),
				   "r" (t->thread_entry())
				 );

	enter_kdebug("blub?");

	return 0;
}

#if SPLIT_HANDLING

struct SplitInfo {
	Romain::InstanceManager *m;
	Romain::App_instance    *i;
	Romain::App_thread      *t;
	Romain::Thread_group    *tg;
	Romain::App_model       *a;
	l4_cap_idx_t             cap;

	SplitInfo()
		: m(0), i(0), t(0), tg(0), a(0), cap(L4_INVALID_CAP)
	{ }

};

#define SYNC_IPC 1

class SplitHandler
{
	l4_cap_idx_t             _split_handler;
	Romain::InstanceManager *_im;
	Romain::Replicator       _replicator;
	SplitInfo **             _psi;
	l4_umword_t*          _checksums;

	void wait_for_instances()
	{
		for (l4_umword_t cnt = 0; cnt < _im->instance_count(); ++cnt) {
#if SYNC_IPC
			l4_umword_t label  = 0;
			l4_msgtag_t t      = l4_ipc_wait(l4_utcb(), &label, L4_IPC_NEVER);
			//MSG() << "Split handler notified: " << std::hex << t.label();
			_psi[cnt]          = (SplitInfo*)l4_utcb_mr()->mr[0];
#else
			MSG() << (void*)&_psi[cnt] << " " << _psi[cnt];

			while (_psi[cnt] == 0) {
				//MSG() << (void*)_psi[cnt];
				l4_thread_yield();
			}

			MSG() << (void*)&_psi[cnt] << " " << _psi[cnt] << "  Split handler notified";
#endif
			_checksums[cnt]    = _psi[cnt]->t->csum_state();
		}

	}


	bool validate_instances()
	{
		for (l4_umword_t cnt = 1; cnt < _im->instance_count(); ++cnt) {
			if (_checksums[cnt] != _checksums[cnt-1]) {
				ERROR() << std::hex << _checksums[cnt] << " != " << _checksums[cnt-1] << "\n";
				ERROR() << "State mismatch detected!\n";
				ERROR() << "=== vCPU states ===\n";
				
				for (l4_umword_t i = 0; i < _im->instance_count(); ++i) {
					ERROR() << "Instance " << _psi[i]->i->id() << " "
					        << "csum " << std::hex << _psi[i]->t->csum_state() << "\n";
					_psi[i]->t->print_vcpu_state();
				}

				return false;
			}
		}
		return true;
	}

	void handle_fault()
	{
		L4vcpu::Vcpu *vcpu = _psi[0]->t->vcpu();
		l4_umword_t trap      = _psi[0]->t->vcpu()->r()->trapno;

		while (trap) {
			MSGi(_psi[0]->i) << BOLD_YELLOW << "TRAP 0x"
			                 << std::hex << vcpu->r()->trapno
			                 << " @ 0x" << vcpu->r()->ip << NOCOLOR;

			Romain::Observer::ObserverReturnVal v 
				= _im->fault_notify(_psi[0]->i, _psi[0]->t, _psi[0]->tg, _psi[0]->a);

			switch(v) {
				case Romain::Observer::Finished:
					{
						for (l4_umword_t c = 1; c < _im->instance_count(); ++c) {
							_im->fault_notify(_psi[c]->i, _psi[c]->t, _psi[c]->tg, _psi[c]->a);
						}
					}
					break;
				case Romain::Observer::Replicatable:
					{
						_replicator.put(_psi[0]->t);
						for (l4_umword_t c = 1; c < _im->instance_count(); ++c) {
							_replicator.get(_psi[c]->t);
						}
					}
					break;
				default:
					enter_kdebug("notify?");
					break;
			}

			if ((trap = _psi[0]->t->get_pending_trap()) != 0)
				_psi[0]->t->vcpu()->r()->trapno = trap;
		}
	}


	void resume_instances()
	{
		for (l4_umword_t c = 0; c < _im->instance_count(); ++c) {
			MSGi(_psi[c]->i) << "Resuming instance @ " << std::hex << _psi[c]->t->vcpu()->r()->ip;
#if SYNC_IPC
			l4_ipc_send(_psi[c]->cap, l4_utcb(), l4_msgtag(0,0,0,0), L4_IPC_NEVER);
#else
			_psi[c]    = 0;
#endif
		}
		MSG() << "... resumed.";
	}

	public:

		static SplitHandler*     _handlers[5];

		static SplitHandler* get(l4_umword_t idx)
		{
			assert(idx == 0); // for now
			return _handlers[idx];
		}

		void notify(Romain::App_instance* i,
		            Romain::App_thread* t,
					Romain::Thread_group* tg,
		            Romain::App_model* a)
		{
#if SYNC_IPC
			SplitInfo si;

			//MSGi(i) << "split handler is " << std::hex << SplitHandler::split_handler_cap();

			//si.m   = _im;
			si.i   = i;
			si.t   = t;
			si.tg  = tg;
			si.a   = a;
			si.cap = t->vcpu_cap().cap();

			l4_utcb_mr()->mr[0] = (l4_umword_t)&si;
			l4_msgtag_t tag = l4_msgtag(0xF00, 1, 0, 0);
			l4_msgtag_t res = l4_ipc_call(SplitHandler::split_handler_cap(),
			                              l4_utcb(), tag, L4_IPC_NEVER);
#else
			SplitInfo si;
			si.i = i;
			si.t = t;
			si.tg = tg;
			si.a = a;
			psi(i->id(), &si);
			MSG() << (void*)&_psi[i->id()] << " " << (void*)&si;

			while (_psi[i->id()] != 0) {
				l4_thread_yield();
			}
#endif
			MSGi(i) << "handled.";
		}

		SplitHandler(Romain::InstanceManager* im)
			: _split_handler(pthread_l4_cap(pthread_self())),
		      _im(im), _replicator()
		{
			_psi       = new SplitInfo*[_im->instance_count()];
			_checksums = new l4_umword_t[_im->instance_count()];
			memset(_psi, 0, sizeof(SplitInfo*) * _im->instance_count());
		}


		~SplitHandler()
		{
			delete [] _psi;
			delete [] _checksums;
		}


		void run()
		{
			MSG() << "Instance mgr: " << (void*)_im;
			MSG() << "Instances: " << _im->instance_count();

			while (true) {
				wait_for_instances();

			DEBUG() << "received faults from " << _im->instance_count()
			        << " instances...";

				if (!validate_instances())
					enter_kdebug("recover");

				handle_fault();

				resume_instances();
			}
		}

		void psi(l4_umword_t idx, SplitInfo* si)
		{ _psi[idx] = si; }

		l4_cap_idx_t split_handler_cap()
		{ return _split_handler; }
};


SplitHandler* SplitHandler::_handlers[5] = {0};


EXTERN_C void *split_handler_fn(void* data)
{
	//SplitHandler::split_handler_cap(pthread_l4_cap(pthread_self()));

	SplitHandler::_handlers[0] = new SplitHandler(reinterpret_cast<Romain::InstanceManager*>(data));
	SplitHandler::get(0)->run();

	enter_kdebug("split_handler terminated!");
	return 0;
}

#endif // SPLIT_HANDLING

void __attribute__((noreturn)) Romain::InstanceManager::VCPU_startup(Romain::InstanceManager *m,
                                                                     Romain::App_instance *i,
                                                                     Romain::App_thread *t,
                                                                     Romain::Thread_group *tg,
                                                                     Romain::App_model*am)
{
	L4vcpu::Vcpu *vcpu = t->vcpu();
	vcpu->task(i->vcpu_task());

	char namebuf[16];
	snprintf(namebuf, 16, "%s.%ld", tg->name.c_str(), i->id());
	l4_debugger_set_object_name(t->vcpu_cap().cap(), namebuf);
	DEBUG() << std::hex << (l4_umword_t)t->vcpu_cap().cap() << " = "
	        << l4_debugger_global_id(t->vcpu_cap().cap())
	        << " ->" << namebuf;

	tg->ready();

	if (t->gdt_changed()) {
		t->commit_client_gdt();
	}

	struct timeval tv;
	gettimeofday(&tv, 0);
	if (Romain::Log::withtime)
		INFO() << BOLD_YELLOW << "Starting @ " << tv.tv_sec << "." << tv.tv_usec << NOCOLOR;
	else
		INFO() << BOLD_YELLOW << "Starting" << NOCOLOR;

	MSGit(i,tg) << "Resuming instance @ " << (void*)vcpu->r()->ip << " ...";

#if EVENT_LOGGING
	Measurements::GenericEvent* ev = Romain::globalLogBuf->next();
	ev->header.tsc                 = Romain::globalLogBuf->getTime(Log::logLocalTSC);
	ev->header.vcpu                = (l4_uint32_t)vcpu;
	ev->header.type                = Measurements::Thread_start;
	ev->data.threadstart.startEIP  = vcpu->r()->ip;
#endif

#if WATCHDOG
	/* Enable watchdog for this replica */
	if (tg->watchdog->enabled()) {
		if (tg->watchdog->enable(i, t))
			INFO() << "Watchdog enabled for instance " << i->id();
		else
			INFO() << "Failed to enable watchdog!";
	}
#endif

	t->ts_user_resume(true);

	L4::Cap<L4::Thread> cap = t->vcpu_cap();
	cap->vcpu_resume_commit(cap->vcpu_resume_start());
	enter_kdebug("startup: after resume");
	l4_sleep_forever();
}


static void local_vCPU_handling(Romain::InstanceManager *m,
                                Romain::App_instance *i,
                                Romain::App_thread *t,
                                Romain::Thread_group *tg,
                                Romain::App_model *a)
{
	// XXX: At this point we might want to reset the GDT to the view that is
	//      expected by the master task, which might differ from the client.

	L4vcpu::Vcpu *vcpu = t->vcpu();
	l4_umword_t trap = t->vcpu()->r()->trapno;
#if BENCHMARKING
	t->ts_from_user();
#endif

	/*
	 * We potentially handle multiple traps here: As we are emulating a bunch
	 * of instructions (e.g., write PF emulation), some real instructions are never
	 * actually executed and may thus not raise certain exceptions such as the INT1 single
	 * step exception. Observers emulating behavior need to be aware of that and inject
	 * a new exception in such circumstances.
	 */
	while (trap) {
		static int x=0;
		if (!x) {
			++x;
			INFO() << "first " << i->id();
		}
		MSGit(i,tg) << BOLD_YELLOW << "TRAP 0x" << std::hex << vcpu->r()->trapno
		        << " @ 0x" << vcpu->r()->ip << NOCOLOR;

		if (t->vcpu()->r()->ip == 0xA041) {
			m->fault_notify(i,t,tg,a);
			break;
		}

#if WATCHDOG
		Romain::RedundancyCallback::EnterReturnVal wrv = Romain::RedundancyCallback::Invalid;
		if (tg->watchdog->enabled()) {
			if (t->watchdog_ss())
				wrv = tg->watchdog->single_stepping(i, t, a);
			if (t->watchdog_breakpointing())
				wrv = tg->watchdog->breakpointing(i, t, a);
		}
#endif

#if 0
		/*
		 * HACK: In case we are using lock-internal determinism, there's a special
		 *       entry address in which a single replica will signal us if we need
		 *       to wake up another thread waiting for a replica. If we encounter
		 *       this address, we skip redundancy handling and directly notify the
		 *       fault handler(s).
		 */
		if (vcpu->r()->ip == 0xA041) {
			// shortcut for unlock()
			m->fault_notify(i,t,tg,a);
			break;
		}
#endif

		Romain::Observer::ObserverReturnVal v         = Romain::Observer::Invalid;
		Romain::RedundancyCallback::EnterReturnVal rv;
		/*
		 * Enter redundancy mode. May cause vCPU to block until leader vCPU executed
		 * its handlers.
		 */
#if WATCHDOG
		if (!tg->watchdog->enabled()) {
			rv = tg->redundancyCB->enter(i,t,tg,a);
		} else {
			if (wrv != Romain::RedundancyCallback::Watchdog) {
				rv = tg->redundancyCB->enter(i,t,tg,a);
		//MSGi(i) << "red::enter: " << rv;
			} else {
				rv = wrv;
			}
		}
#else
		rv = tg->redundancyCB->enter(i,t,tg,a);
#endif

		t->ts_sync_leave();

		/*
		 * Case 1: we are the first to exec this system call.
		 */
		if (rv == Romain::RedundancyCallback::First_syscall) {
			v = m->fault_notify(i,t,tg,a);
			MSGit(i,tg) << "fault_notify: " << v;
			switch(v) {
				case Romain::Observer::Finished:
				case Romain::Observer::Finished_wait:
				case Romain::Observer::Finished_step:
				case Romain::Observer::Finished_wakeup:
					//MSGi(i) << "leader_repeat()";
					tg->redundancyCB->leader_repeat(i,t,a);
					break;
				case Romain::Observer::Replicatable:
					tg->redundancyCB->leader_replicate(i,t,a);
					break;
				default:
					//enter_kdebug("fault was not finished/replicatable");
					break;
			}
		}
		/*
		 * Case 2: leader told us to do the syscall ourselves
		 */
		else if (rv == Romain::RedundancyCallback::Repeat_syscall) {
			v = m->fault_notify(i,t,tg,a);
			MSGit(i,tg) << "fault_notify: " << v;
		}

		/*
		 * Special handling for the Finished_{wakeup,wait,step} cases
		 * used by the fault injector.
		 */
		switch(v) {
			case Romain::Observer::Finished_wait:
				// go to sleep until woken up
				tg->redundancyCB->wait(i,t,a);
				break;
			case Romain::Observer::Finished_wakeup:
				// wakeup -> first needs to ensure that all other
				// vCPUs are actually waiting
				tg->redundancyCB->silence(i,t,a);
				tg->redundancyCB->wakeup(i,t,a);
				break;
			case Romain::Observer::Finished_step:
				// let other vCPUs step until they go to sleep
				tg->redundancyCB->silence(i,t,a);
				break;
			default:
				break;
		}

		if ((trap = t->get_pending_trap()) != 0)
			t->vcpu()->r()->trapno = trap;

		t->ts_resume_start();
		tg->redundancyCB->resume(i, t, tg, a);
	}

	//print_vcpu_state();
    MSGit(i, tg) << "Resuming instance @ " << std::hex << t->vcpu()->r()->ip;
	//t->print_vcpu_state();
}


#if SPLIT_HANDLING
static void split_vCPU_handling(Romain::InstanceManager *m,
                                Romain::App_instance *i,
                                Romain::App_thread *t,
                                Romain::Thread_group *tg,
                                Romain::App_model *a)
{
	MSG() << "here";
	SplitHandler::get(0)->notify(i,t,a);
}
#endif // SPLIT_HANDLING


#if MIGRATE_VCPU
static void migrated_vCPU_handling(Romain::InstanceManager *m,
                                   Romain::App_instance *i,
                                   Romain::App_thread *t,
                                   Romain::Thread_group *tg,
                                   Romain::App_model *a)
{
	l4_sched_param_t sp = l4_sched_param(2);
	sp.affinity = l4_sched_cpu_set(0, 0);
	chksys(L4Re::Env::env()->scheduler()->run_thread(t->vcpu_cap(),
	                                                 sp));

	local_vCPU_handling(m, i, t, a);

	sp = l4_sched_param(2);
	sp.affinity = l4_sched_cpu_set(t->cpu(), 0);
	chksys(L4Re::Env::env()->scheduler()->run_thread(t->vcpu_cap(),
	                                                 sp));
}
#endif // MIGRATE_VCPU

/*
 * VCPU fault entry point.
 *
 * Calls the registered observers and keeps track of any further faults that might get
 * injected during handler execution.
 */
void __attribute__((noreturn)) Romain::InstanceManager::VCPU_handler(Romain::InstanceManager *m,
                                                                     Romain::App_instance *i,
                                                                     Romain::App_thread *t,
                                                                     Romain::Thread_group *tg,
                                                                     Romain::App_model *a)
{
	L4vcpu::Vcpu *vcpu = t->vcpu();
	vcpu->state()->clear(L4_VCPU_F_EXCEPTIONS | L4_VCPU_F_DEBUG_EXC);
	handler_prolog(t);
	unsigned long long t1, t2;

#if EVENT_LOGGING
	Measurements::GenericEvent* ev = Romain::globalLogBuf->next();
	ev->header.tsc     = Romain::globalLogBuf->getTime(Log::logLocalTSC);
	ev->header.vcpu        = (l4_uint32_t)vcpu;
	ev->header.type        = Measurements::Trap;
	ev->data.trap.start    = 1;
	ev->data.trap.trapaddr = vcpu->r()->ip;
	ev->data.trap.trapno   = vcpu->r()->trapno;
#endif

#if BENCHMARKING
	unsigned long long t1, t2;
	t1 = l4_rdtsc();
#endif

	t1 = l4_rdtsc();
#if MIGRATE_VCPU
	migrated_vCPU_handling(m, i, t, tg, a);
#elif SPLIT_HANDLING
	split_vCPU_handling(m, i, t, tg, a);
#elif LOCAL_HANDLING
	local_vCPU_handling(m, i, t, tg, a);
#else
#error No vCPU handling method selected!
#endif

#if WATCHDOG
	if (tg->watchdog->enabled())
		tg->watchdog->reset(i, t, t->watchdog_timeout());
#endif
	t2 = l4_rdtsc();

#if BENCHMARKING
	t2 = l4_rdtsc();
	t->count_handling(t2-t1);
#endif

	if (t->gdt_changed()) {
		t->commit_client_gdt();
	}

#if EVENT_LOGGING
	ev = Romain::globalLogBuf->next();
	ev->header.tsc     = Romain::globalLogBuf->getTime(Log::logLocalTSC);
	ev->header.vcpu      = (l4_uint32_t)vcpu;
	ev->header.type      = Measurements::Trap;
	ev->data.trap.start  = 0;
	ev->data.trap.trapno = ~0U;
#endif

	t->ts_user_resume();
	L4::Cap<L4::Thread> self;
	self->vcpu_resume_commit(self->vcpu_resume_start());

	enter_kdebug("notify: after resume");
	l4_sleep_forever();
}

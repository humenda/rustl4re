/*
 * app_thread.cc --
 *
 *     App_thread functions for creating and preparing a new VCPU
 *
 * (c) 2011-2013 Björn Döbel <doebel@os.inf.tu-dresden.de>,
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include "app"
#include "app_loading"
#include "thread_group.h"

#include <l4/libloader/remote_app_model>
#include <pthread-l4.h>

void Romain::App_thread::alloc_vcpu_mem()
{
	/* Alloc vUTCB */
	l4_addr_t kumem;
	L4Re::Util::kumem_alloc(&kumem, 0);
	_check(kumem == 0, "out of memory in kumem_alloc");

	_vcpu_utcb = (l4_utcb_t *)kumem;
	_vcpu = L4vcpu::Vcpu::cast(kumem + L4_UTCB_OFFSET);

	/* store segment registers - stolen from the example */
	//_vcpu->r()->gs = _master_ds;
	_vcpu->r()->fs = _master_ds & 0xFF;
	_vcpu->r()->es = _master_ds & 0xFF;
	_vcpu->r()->ds = _master_ds & 0xFF;
	_vcpu->r()->ss = _master_ds & 0xFF;

	/* We want to catch ALL exceptions for this vCPU. */
	_vcpu->saved_state()->set(
	                          L4_VCPU_F_IRQ
	                          | L4_VCPU_F_PAGE_FAULTS
	                          | L4_VCPU_F_EXCEPTIONS
	                          | L4_VCPU_F_DEBUG_EXC
	                          | L4_VCPU_F_USER_MODE
	                          | L4_VCPU_F_FPU_ENABLED
	);

	DEBUG() << "VCPU: utcb = " << (void*)vcpu_utcb()
	        << " vcpu @ " << (void*)vcpu();
}


void Romain::App_thread::touch_stacks()
{
	/* We need to touch at least the handler stack, because upon entry, the vCPU
	 * still has interrupts enabled and we must not raise one by causing a page
	 * fault on the stack area.
	 */
	DEBUG() << "Stack info:";
	DEBUG() << "   handler stack @ " << (void*)_handler_stack
	        << " - " << (void*)(_handler_stack + sizeof(_handler_stack));

	l4_touch_rw(_handler_stack, sizeof(_handler_stack));
}


void Romain::App_thread::alloc_vcpu_cap()
{
#if 0
	_vcpu_cap = chkcap(L4Re::Util::cap_alloc.alloc<L4::Thread>(),
	                   "vCPU cap alloc");
	chksys(L4Re::Env::env()->factory()->create(_vcpu_cap),
	       "create thread");
	l4_debugger_set_object_name(_vcpu_cap.cap(), "vcpu thread");
#endif
}


extern "C" void* pthread_fn(void *);

/*
 * Create a replica thread
 */
void Romain::App_thread::start()
{
	/*
	 * We only set handler IP and SP here, because beforehand our creator
	 * may have modified them.
	 */
	_vcpu->entry_sp(handler_sp());
	_vcpu->entry_ip((l4_umword_t)_handler_fn);

	l4_mword_t err = pthread_create(&_pthread, NULL, pthread_fn, this);
	_check(err != 0, "pthread_create");

	//_vcpu_cap = Pthread::L4::cap(_pthread);
}


/*
 * Calculate checksum of the replica's state
 *
 * This checksum is used to compare replica states.
 */
l4_umword_t
Romain::App_thread::csum_state()
{
	// XXX: this should also include the UTCB, which
	//      means the _used_ part of the UTCB
	return _vcpu->r()->ip
	     + _vcpu->r()->sp
	     + _vcpu->r()->ax
	     + _vcpu->r()->bx
	     + _vcpu->r()->cx
	     + _vcpu->r()->dx
	     + _vcpu->r()->bp
	     /*+ _vcpu->r()->fs
	     + _vcpu->r()->gs*/
	     ;
}


void Romain::App_thread::commit_client_gdt()
{
	vcpu()->r()->fs = fiasco_gdt_set(vcpu_cap().cap(), gdt(), gdt_size(), 1, l4_utcb());

	if (_client_gdt[1].base_low != 0)
		vcpu()->r()->gs = vcpu()->r()->fs + 8;

	DEBUG() << "CPU " << cpu() << " FS " << std::hex << vcpu()->r()->fs;
	DEBUG() << "CPU " << cpu() << " GS " << std::hex << vcpu()->r()->gs;
	//vcpu()->print_state();
	_gdt_modified = false;
}


void
Romain::Thread_group::ex_regs(Romain::App_thread *caller)
{
	/*
	 * Thoughts on ex_regs
	 *
	 * One use case of ex_regs is for an external thread to halt execution
	 * of a thread, store the halted thread's state and later continue from
	 * this point. As replicated threads execute independently, we will not
	 * stop all replicas in the same state. However, we require the replicas
	 * to be in the same state on resumption.
	 *
	 * To make things deterministic, we stop all replicas. As a heuristic, we
	 * then assume that the replicas are all correct (as all state
	 * difference can be attributed to diverging execution). We select the last
	 * stopped replica (potentially being the most advanced) as good copy and
	 * copy its state over to all other replicas, thereby forcing them to the
	 * same state.
	 *
	 * 1) This may shadow certain errors that occurred prior to the ex_regs.
	 *    -> we need to evaluate how often that happens
	 *
	 * 2) HP NonStop instead executes replicas in lock-step until it can
	 *    figure out which is the most advanced. THen they execute everyone
	 *    else up to this point.
	 *    -> higher overhead
	 *    -> no lack in error detection
	 *
	 * 3) Can we detect situations where this happens as hard overwrite of
	 *    the replicated thread's state -> hence we would not need to first
	 *    enforce identical states? -> heuristics only, cannot determine
	 *    upfront if the caller will later reuse the state.
	 *
	 */
	bool need_to_stop = !this->stopped;

	if (need_to_stop) {
		enter_kdebug("stop thread magic goes here");
	}

	l4_msg_regs_t *buf = l4_utcb_mr_u(reinterpret_cast<l4_utcb_t*>(caller->remote_utcb()));
	DEBUG() << std::hex << buf->mr[0];
	DEBUG() << std::hex << buf->mr[1];
	DEBUG() << std::hex << buf->mr[2];

	Romain::App_thread* thread = threads[0];
	l4_umword_t eip = thread->vcpu()->r()->ip;
	l4_umword_t esp = thread->vcpu()->r()->sp;

	DEBUG() << (void*)thread->vcpu();
	for (l4_umword_t i = 0; i < threads.size(); ++i) {
		threads[i]->vcpu()->r()->ip = buf->mr[1];
		threads[i]->vcpu()->r()->sp = buf->mr[2];
	}

	buf->mr[1] = eip;
	buf->mr[2] = esp;

	caller->vcpu()->r()->ax = l4_msgtag(0,3,0,0).raw;

	if (need_to_stop) {
		/*
		 * If we stopped the threads before,
		 * we need to reactivate them now.
		 */
		enter_kdebug("reactivation magic goes here");
	}
	//enter_kdebug("Thread_group::ex_regs");
}


void
Romain::Thread_group::scheduler_run(Romain::App_thread *caller)
{
	l4_msg_regs_t *buf = l4_utcb_mr_u(reinterpret_cast<l4_utcb_t*>(caller->remote_utcb()));
	
	DEBUG() << "Thread_group::scheduler_run("
	        << (void*)threads[0]->vcpu()
	        << ")";
	DEBUG() << "granularity | affinity.offs : " << std::hex << buf->mr[1];
	DEBUG() << "affinity.map                : " << std::hex << buf->mr[2];
	DEBUG() << "prio                        : " << std::hex << buf->mr[3];
	DEBUG() << "quantum                     : " << std::hex << buf->mr[4];
	DEBUG() << "obj_control                 : " << std::hex << buf->mr[5];
	DEBUG() << "thread                      : " << std::hex << buf->mr[6];

	activate();

	//enter_kdebug("Thread_group::schedule()");
}


void
Romain::Thread_group::sanity_check_control(l4_umword_t flags, l4_utcb_t *utcb)
{
	DEBUG() << "Control flags: " << std::hex << l4_utcb_mr_u(utcb)->mr[L4_THREAD_CONTROL_MR_IDX_FLAGS];

	if ((flags & L4_THREAD_CONTROL_ALIEN) ||
		(flags & L4_THREAD_CONTROL_UX_NATIVE)) {
		ERROR() << "ux_native and alien not supported yet\n";
	}

	if (flags & L4_THREAD_CONTROL_BIND_TASK) {
		l4_fpage_t fp;
		fp.raw = l4_utcb_mr_u(utcb)->mr[L4_THREAD_CONTROL_MR_IDX_BIND_TASK + 1];
                if (l4_fpage_type(fp) != L4_FPAGE_OBJ)
                  return;

		DEBUG() << std::hex << "L4_THREAD_CONTROL_BIND_TASK := ("
		        << l4_utcb_mr_u(utcb)->mr[L4_THREAD_CONTROL_MR_IDX_BIND_UTCB] << ", "
		        << l4_fpage_obj(fp) << ")";
		if (l4_fpage_obj(fp) != L4Re::This_task) {
			ERROR() << "Binding to different task not supported yet.\n";
			enter_kdebug("error");
		}
		/* Apart from these checks, don't do anything. The replica vCPUs
		 * are already bound.
		 */
	}

	l4_umword_t handler = 0;
	l4_umword_t pager   = 0;

	if (flags & L4_THREAD_CONTROL_SET_PAGER) {
		pager = l4_utcb_mr_u(utcb)->mr[L4_THREAD_CONTROL_MR_IDX_PAGER];
		DEBUG() << "pager <- " << std::hex << pager;
	}

	if (flags & L4_THREAD_CONTROL_SET_EXC_HANDLER) {
		handler = l4_utcb_mr_u(utcb)->mr[L4_THREAD_CONTROL_MR_IDX_EXC_HANDLER];
		DEBUG() << "exc handler <- " << std::hex << handler;
	}

	if (handler && pager) {
		if ((handler != pager)  || 
			(handler != Ldr::Remote_app_std_caps::Rm_thread_cap << L4_CAP_SHIFT) ||
			(pager != Ldr::Remote_app_std_caps::Rm_thread_cap << L4_CAP_SHIFT)) {
			ERROR() << "setting different pager or exc. handler not supported yet\n";
			enter_kdebug("error");
		}
	}

	if (handler && (handler != Ldr::Remote_app_std_caps::Rm_thread_cap << L4_CAP_SHIFT)) {
		ERROR() << "setting non-standard pager not supported yet\n";
	}

	if (pager && (pager != Ldr::Remote_app_std_caps::Rm_thread_cap << L4_CAP_SHIFT)) {
		ERROR() << "setting non-standard exc. handler not supported yet\n";
	}
}


void
Romain::Thread_group::control(Romain::App_thread *t, l4_utcb_t *utcb, Romain::App_model *am)
{
	l4_umword_t flags = l4_utcb_mr_u(utcb)->mr[L4_THREAD_CONTROL_MR_IDX_FLAGS];

	sanity_check_control(flags, utcb);

	if (flags & L4_THREAD_CONTROL_BIND_TASK) {
		l4_addr_t utcb_remote = l4_utcb_mr_u(utcb)->mr[L4_THREAD_CONTROL_MR_IDX_BIND_UTCB];
		DEBUG() << "Setting remote UTCB to " << (void*)utcb_remote;

		L4::Cap<L4Re::Dataspace> mem;
		Romain::Region_map::allocate_ds(&mem, L4_PAGESIZE);
		l4_addr_t local_addr = Romain::Region_map::allocate_and_attach(&mem, L4_PAGESIZE, 0, 0);
		DEBUG() << "Attached TIP to " << (void*)local_addr;

		/* store UTCB address in TIP */
		*reinterpret_cast<l4_umword_t*>(local_addr) = utcb_remote;

		am->rm()->activate(0);
		void* tip_addr = (void*)0x10000;
		tip_addr = am->rm()->attach(tip_addr, L4_PAGESIZE,
		                            Romain::Region_handler(mem, L4_INVALID_CAP, 0,
		                                                   0, Romain::Region(local_addr, local_addr + L4_PAGESIZE - 1)),
		                            L4Re::Rm::Search_addr, L4_PAGESHIFT, false);
		am->rm()->release();

		INFO() << "Remote TIP address: " << tip_addr;

		for (l4_umword_t i = 0; i < threads.size(); ++i) {
			threads[i]->setup_utcb_segdesc(reinterpret_cast<l4_addr_t>(tip_addr), 4);

			l4_addr_t utcb_local = am->rm()->remote_to_local(utcb_remote, i);
			threads[i]->remote_utcb(utcb_local);
			//threads[i]->commit_client_gdt();
		}

		//enter_kdebug("utcb");
	}

	/*
	 * Our current assumption is that the replicated app uses only default
	 * threading features, e.g. does not change pager, exception handler or
	 * any other thread features. Therefore, after sanity checking for these
	 * assumptions, we simply pretend everything went alright.
	 */
	t->vcpu()->r()->ax = l4_msgtag(0, 3, 0, 0).raw;
}

void
Romain::Thread_group::gdt(Romain::App_thread* t, l4_utcb_t *utcb)
{
	enum { replica_gs_base = 0x58 };

	l4_msgtag_t *tag = reinterpret_cast<l4_msgtag_t*>(&t->vcpu()->r()->ax);
	DEBUG() << BOLD_BLUE << "GDT: words" << NOCOLOR << " = " << tag->words();

	// 1 word -> query GDT start
	if (tag->words() == 1) {
		l4_utcb_mr_u(utcb)->mr[0] = replica_gs_base >> 3;
		t->vcpu()->r()->ax = l4_msgtag(0, 1, 0, 0).raw;
		enter_kdebug("gdt query");
	} else { // setup new GDT entry
		l4_umword_t idx      = l4_utcb_mr_u(utcb)->mr[1];
		l4_umword_t numbytes = (tag->words() == 4) ? 8 : 16;

		for (l4_umword_t i = 0; i < threads.size(); ++i) {
			Romain::App_thread* thread = threads[i];

			if ((idx == 0) and (numbytes == 8)) { // actually, we only support a single entry here
				thread->write_gdt_entry(&l4_utcb_mr_u(utcb)->mr[2], numbytes);
				DEBUG() << "GS: " << std::hex << thread->vcpu()->r()->gs;
			} else {
				enter_kdebug("GDT??");
			}
		}

		t->vcpu()->r()->ax = l4_msgtag((idx << 3) + replica_gs_base + 3, 0, 0, 0).raw;
	}
}


void* Romain::GateAgent::listener_function(void *gk)
{
	GateAgent *agent = reinterpret_cast<GateAgent*>(gk);
	static char* utcb_copy[L4_UTCB_OFFSET];

	char namebuf[16];
	snprintf(namebuf, 16, "GK::%s", agent->owner_group->name.c_str());
	l4_debugger_set_object_name(pthread_l4_cap(pthread_self()), namebuf);

	sem_wait(&agent->init_sem);
	sem_destroy(&agent->init_sem);
	DEBUG() << "starting agent loop";

	l4_utcb_t *my_utcb = l4_utcb();
	l4_msgtag_t tag;

	while (1) {
#if USE_SHMSYNC
		while (agent->current_client == 0) {
			l4_thread_yield();
		}
#endif
		
#if USE_IRQ
		tag = agent->gate_irq->receive();
#endif

#if 0
		DEBUG() << "Keeper activated by replica.";
		DEBUG() << "agent " << (void*)agent;
		DEBUG() << "client " << (void*)agent->current_client;
		DEBUG() << " CLNT: " << (void*)agent->current_client << " remote_utcb: "
		        << (void*)agent->current_client->remote_utcb();
		if (agent->current_client == 0) {
			DEBUG() << "!!!!!" << std::hex << " " << tag.raw;
			DEBUG() << l4sys_errtostr(l4_error(tag));
			enter_kdebug();
			continue;
		}
		_check(agent->current_client == 0, "agent called with client NULL?");
#endif
		
		memcpy(utcb_copy, my_utcb, L4_UTCB_OFFSET);
		memcpy(my_utcb, (void*)agent->current_client->remote_utcb(), L4_UTCB_OFFSET);

		//outhex32((l4_umword_t)agent->current_client->vcpu()); outstring(" enter kernel\n");
		asm volatile( L4_ENTER_KERNEL
			  : "=a" (agent->current_client->vcpu()->r()->ax),
		        "=b" (agent->current_client->vcpu()->r()->bx),
		        /* ECX, EDX are overwritten anyway */
		        "=S" (agent->current_client->vcpu()->r()->si),
		        "=D" (agent->current_client->vcpu()->r()->di)
			  : "a" (agent->current_client->vcpu()->r()->ax),
		        /* EBX and EBP will be overwritten with local
		         * values in L4_ENTER_KERNEL */
		        "c" (agent->current_client->vcpu()->r()->cx),
		        "d" (agent->current_client->vcpu()->r()->dx),
		        "S" (agent->current_client->vcpu()->r()->si),
		        "D" (agent->current_client->vcpu()->r()->di)
                        L4S_PIC_SYSCALL
			  : "memory", "cc"
		);
#if 0
		outhex32((l4_umword_t)agent->current_client->vcpu()); outstring(" ret from kernel ");
		outhex32((l4_umword_t)agent->current_client->vcpu()->r()->ax); outstring("\n");
#endif

		memcpy((void*)agent->current_client->remote_utcb(), my_utcb, L4_UTCB_OFFSET);
		memcpy(my_utcb, utcb_copy, L4_UTCB_OFFSET);

		l4_cap_idx_t cap = agent->current_client->vcpu_cap().cap();
		agent->current_client = 0;
		tag = l4_ipc_send(cap, l4_utcb(), l4_msgtag(0,0,0,0), L4_IPC_NEVER);
	}

	enter_kdebug("gateagent exited");

	return 0;
}

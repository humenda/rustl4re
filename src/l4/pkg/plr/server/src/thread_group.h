#pragma once

/*
 * thread_group.h --
 *
 *    Thread group -> representation of a single replicated thread
 *
 * (c) 2012-2013 Björn Döbel <doebel@os.inf.tu-dresden.de>,
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */


#include "app"
#include "app_loading"

#include <l4/sys/ipc.h>

#include <pthread.h>
#include <pthread-l4.h>
#include <semaphore.h>

namespace Romain
{

class RedundancyCallback;
	
#if WATCHDOG
class Watchdog;
#endif
	
struct Thread_group;

/*
 * Acting instance for a thread group
 *
 * A replicated thread does not possess a unique capability through which it is
 * accessible. However, Fiasco allows others to send a message to a thread
 * using its thread capability directly instead of an IPC gate. In order to
 * allow for this in the context of replicated thread groups, we replace the
 * original cap slot with an IPC gate that receives all these messages. A
 * dedicated thread, the GateAgent, is attached to this gate. It waits for
 * messages and delivers them to the replicas.
 *
 * Q: Does the GateAgent receive early / out-of-order messages?
 * A: No. The GateAgent only performs an operation if the replicated thread group
 *    decided to do so. It then only acts as their agent.
 *
 * Q: Do we need to distinguish external send and call operations?
 * A: No. A reply will only be sent if the thread group decides so. The gateagent will
 *    use the proper function required in every case.
 *
 * Q: IPC wait() does not specify a gate to wait on. How do we handle this?
 * A: We intercept all calls that bind the thread to a newly created gate. In
 *    every case, instead of binding a real thread, we bind the GateAgent to this
 *    gate. Thereby, an open wait is triggered by the thread group, but always
 *    carried out by the agent and therefore all messages arrive in the right
 *    place.
 *    
 * Q: How do we handle IRQs / explicit receive gates?
 * A: The gate agent also handles those. To do so, it needs to be attached to these
 *    objects (as opposed to the original threads). Therefore, we need to intercept
 *    IPC gate creation (see the factory syscall handler) and IRQ object attachment
 *    (see syscall handling) and adapt their parameters to use the thread group's
 *    gate agent.
 */

#define USE_SHMSYNC 0
#define USE_IRQ     1

struct GateAgent
{
	enum { GateAgent_label = 0x195300 };

	sem_t                 init_sem;        // semaphore for startup
	L4::Cap<L4::Kobject>  gate_cap;        // IPC gate the agent polls on
#if USE_IRQ
	L4::Cap<L4::Irq>      gate_irq;        // IRQ to notify the agent of pending work
#endif
	pthread_t             listener;        // gate agent thread
	L4::Cap<L4::Thread>   listener_cap;    // gate agent thread cap

	Romain::Thread_group* owner_group;     // thread group this agent belongs to
	Romain::App_thread*   current_client;  // currently handled replica thread (will be
	                                       // one of the group's threads)

	static void *listener_function(void *gk);

	/*
	 * Trigger gate agent (replica -> agent)
	 */
	void trigger_agent(Romain::App_thread *t)
	{
		_check(current_client != 0, "current client not 0");
		current_client = t;
#if USE_SHMSYNC
		while (current_client)
			l4_thread_yield();
#endif

#if USE_IRQ
		gate_irq->trigger();
		l4_umword_t lbl;
		l4_ipc_wait(l4_utcb(), &lbl, L4_IPC_NEVER);
		/* No error check. After return from this call, the UTCB contains
		 * the IPC error the gate agent received, which may in fact be a real
		 * IPC error -> it is up to the app to check and react on it. */
#endif

		DEBUG() << "agent returned.";
	}


	GateAgent(l4_umword_t cap_idx, Romain::Thread_group *tg)
		: owner_group(tg), current_client(0)
	{
		DEBUG() << BOLD_RED << "GateAgent" << NOCOLOR;
		sem_init(&init_sem, 0, 0);
		
		/*
		 * launch listener thread
		 */
		l4_mword_t err  = pthread_create(&listener, 0,
		                                 listener_function, (void*)this);
		_check(err != 0, "error creating listener thread");
		l4_thread_yield();

		listener_cap    = Pthread::L4::cap(listener);
		_check(!listener_cap.is_valid(), "could not get listener pthread cap?");

		/*
		 * create IPC gate
		 */
		gate_cap        = L4::Cap<L4::Kobject>(cap_idx << L4_CAP_SHIFT);
		_check(!gate_cap.is_valid(), "could not create gate");
		l4_msgtag_t tag = L4Re::Env::env()->factory()->create_gate(gate_cap,
		                                                           listener_cap,
		                                                           GateAgent_label);
		if (l4_error(tag)) {
			enter_kdebug("gate creation error");
		}

#if USE_IRQ
		/*
		 * Create notification IRQ
		 * 
		 * Q: Why no pthread semaphore or mutex?
		 * A: Evil things happened. Seems we disrupt some pthreads assumptions
		 *    when doing so in our context.
		 */
		gate_irq = L4Re::Util::cap_alloc.alloc<L4::Irq>();
		//_check(!gate_irq.valid(), "error allocating gate2");
		tag = L4Re::Env::env()->factory()->create(gate_irq);
		if (l4_error(tag)) {
			enter_kdebug("IRQ creation error");
		}
		tag = gate_irq->attach(GateAgent_label+2, listener_cap);
		if (l4_error(tag)) {
			enter_kdebug("attach error");
		}
#endif

		sem_post(&init_sem);
		DEBUG() << "started agent thread";
	}
};

/*
 * Instance of a replicated thread
 *
 * In contrast to an App_instance (which maps to a dedicated address space),
 * a thread group simply keeps track of which App_threads belong together
 * as they run the same original thread code.
 */
struct Thread_group
{
	Thread_group(std::string n, l4_umword_t cap_idx, l4_umword_t u)
		: threads(), name(n), uid(u), stopped(true)
	{
		DEBUG() << BOLD_RED << "Thread_group " << NOCOLOR << std::hex << cap_idx;

		gateagent = new GateAgent(cap_idx, this);

		sem_init(&activation_sem, 0, 0);
	}

	/* Replica threads */
	std::vector<Romain::App_thread*> threads;

	/* DBG: group name, unique ID */
	std::string name;
	l4_umword_t uid;

	/* The group's gate agent. Docs, see there. */
	GateAgent* gateagent;

	/*
	 * Marks the thread group as currently stopped.
	 *
	 * - initially, we start in this state, because threads are created
	 *   using factory_create and later run when activated through the
	 *   scheduler
	 * - later we track stopped states, e.g., when blocking in a system
	 *   call
	 */
	bool stopped;

	/*
	 * Barrier that is used to block vCPU until the thread gets
	 * activated.
	 */
	sem_t activation_sem;


	Romain::RedundancyCallback *redundancyCB;
	void set_redundancy_callback(Romain::RedundancyCallback* cb)
	{ redundancyCB = cb; }
	
#if WATCHDOG
	Romain::Watchdog *watchdog;
	void set_watchdog(Romain::Watchdog *w)
	{ watchdog = w; }
#endif

	void add_replica(App_thread *a)
	{
		threads.push_back(a);
	}


	/* Halt all threads by setting their prio to 0 */
	void halt()
	{
		INFO() << "Halting TG '" << name << "'";
		for (auto it = threads.begin(); it != threads.end(); ++it) {
			(*it)->halt();
		}
	}


	/*
	 * Activate the thread group
	 *
	 * notify all vCPUs blocking on the activation semaphore
	 */
	void activate()
	{
		stopped = false;

		for (l4_umword_t i = 0; i < threads.size(); ++i) {
			sem_post(&activation_sem);
		}
	}


	/*
	 * Lets the current thread indicate being ready to run
	 * by waiting on the activation semapore.
	 */
	void ready()
	{
		sem_wait(&activation_sem);
	}


	Romain::App_thread* get(l4_umword_t number)
	{
		_check(number < threads.size(), "invalid thread instance requested");
		return threads[number];
	}


	void ex_regs(Romain::App_thread *caller);
	void scheduler_run(Romain::App_thread* caller);

	/*
	 * Check if thread_control() was called with parameters
	 * we don't support yet.
	 */
	void sanity_check_control(l4_umword_t flags, l4_utcb_t *utcb);
	void control(Romain::App_thread *t, l4_utcb_t *utcb, Romain::App_model* am);

	void gdt(Romain::App_thread *t, l4_utcb_t *utcb);
};
}

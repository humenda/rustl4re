#pragma once

/*
 * syscalls_factory.h --
 *
 *     Implementation of the factory system call
 *
 * (c) 2012-2013 Björn Döbel <doebel@os.inf.tu-dresden.de>,
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */


#include "syscalls_handler.h"
#include <l4/util/bitops.h>

namespace Romain
{


/*
 * Handler for factory system calls.
 * 
 * This class deals mainly with thread and IRQ objects and keeps track
 * of their capabilities.
 */
class Factory : public SyscallHandler
{
	/* Trace CAP -> thread mappings
	 * 
	 * This allows us to easily find the thread group belonging to a
	 * capability (which in reality maps to the gate agent IPC gate)
	 */
	std::map<l4_umword_t, Romain::Thread_group*> _threads;
	l4_umword_t _thread_count;

	l4_umword_t _irqbits[16]; // trace if certain caps are IRQ objects

	void create_thread(Romain::App_instance *i,
	                   Romain::App_thread *t,
	                   Romain::Thread_group *tg,
	                   Romain::App_model *a,
	                   l4_umword_t cap)
	{
		char threadname[24];
		// writing this in C is sooo much shorter than creating a stringbuf...
		snprintf(threadname, 24, "thread%ld", _thread_count);

		cap >>= L4_CAP_SHIFT;

		MSG() << "Creating new thread! " << _threads.size();
		MSG() << "CAP: " << std::hex << cap;
		Romain::Thread_group* newgroup =
			Romain::_the_instance_manager->create_thread_group(0, 0, threadname,
															   cap, _thread_count);
		MSG() << "Group @ " << (void*)newgroup;

		_thread_count++;
		_threads[cap] = newgroup;

		/* We launch the replica threads now. At this point we must have
		 * made sure that all faults raised by these threads will be resolved,
		 * because we cannot be sure if they run before or after any subsequent
		 * code. */
		//l4_umword_t cnt = 0;
		for (auto it = newgroup->threads.begin();
			 it != newgroup->threads.end(); ++it)
		{
			(*it)->start();
		}

		t->vcpu()->r()->ax = l4_msgtag(0, 1, 0, 0).raw;
		//enter_kdebug("created thread");
	}


	/*
	 * Create IRQ and store that this cap is an IRQ.
	 */
	void create_irq(Romain::App_instance *i,
	                Romain::App_thread *t,
	                Romain::Thread_group *tg,
	                Romain::App_model *a,
	                l4_umword_t cap)
	{
		DEBUG() << "Creating IRQ with cap " << std::hex << (cap >> L4_CAP_SHIFT);
		mark_irq((cap >> L4_CAP_SHIFT) - Romain::FIRST_REPLICA_CAP);
		L4::Cap<L4::Irq> irqcap(cap);
		t->vcpu()->r()->ax = ((l4_msgtag_t)L4Re::Env::env()->factory()->create(irqcap)).raw;
    }


	/*
	 * Store that the given cap points to an IRQ object
	 */
	void mark_irq(l4_umword_t cap)
	{
		DEBUG() << "cap = " << std::hex << cap;
		l4_umword_t *dest  = _irqbits;
		dest           += cap / (sizeof(*dest) * 8);
		cap            &= sizeof(*dest) * 8 - 1;
		*dest          |= (1 << cap);
	}

	public:
		Factory()
			: SyscallHandler(), _threads(), _thread_count(0)
		{ }

		virtual
		Romain::Observer::ObserverReturnVal
		handle(Romain::App_instance *i,
		       Romain::App_thread *t,
		       Romain::Thread_group *tg,
		       Romain::App_model *a);


		void register_thread_group(Romain::Thread_group* tg, l4_umword_t cap)
		{
			_check(cap < Romain::FIRST_REPLICA_CAP, "invalid replica #");
			_thread_count++;
			_threads[cap] = tg;
			DEBUG() << std::hex << cap << " := " << (void*)_threads[cap];
		}


		/*
		 * Get the Thread_group to an agent's gate.
		 */
		Romain::Thread_group* thread_for_cap(l4_umword_t cap)
		{
			DEBUG() << std::hex << cap << " -> " << (void*)_threads[cap];
			return _threads[cap];
		}


		/*
		 * Figure out if a cap belongs to an IRQ object (that was created
		 * through this factory.
		 */
		bool is_irq(l4_umword_t cap)
		{
			cap            >>= L4_CAP_SHIFT;
			if (cap < Romain::FIRST_REPLICA_CAP) {
				return false;
			}
			cap             -= Romain::FIRST_REPLICA_CAP;

			DEBUG() << "cap = " << std::hex << cap;
			l4_umword_t *dest   = _irqbits;
			dest            += cap / (sizeof(*dest) * 8);
			cap             &= sizeof(*dest) * 8 - 1;
			return *dest & (1 << cap);
		}
};

}

extern Romain::Factory theObjectFactory;

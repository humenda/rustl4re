/*
 * lock_observer.cc --
 *
 *     Deterministic lock acquisition
 *
 * (c) 2012-2013 Björn Döbel <doebel@os.inf.tu-dresden.de>,
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include "lock_observer.h"
#include <l4/util/util.h>

#define DEBUGt(t) DEBUG() <<  "[" << t->vcpu() << "] "

#if EVENT_LOGGING
#define EVENT(event, ptr) \
    do { \
	 	Measurements::GenericEvent* ev = Romain::globalLogBuf->next(); \
		ev->header.tsc                 = Romain::globalLogBuf->getTime(Log::logLocalTSC); \
		ev->header.vcpu                = (l4_uint32_t)t->vcpu(); \
		ev->header.type                = Measurements::Locking; \
		ev->data.lock.eventType        = event; \
		ev->data.lock.lockPtr          = ptr; \
    } while (0)
#else
#define EVENT(event, ptr) do {} while (0)
#endif

Romain::PThreadLockObserver*
Romain::PThreadLockObserver::Create()
{ return new PThreadLock_priv(); }

namespace Romain {
	extern InstanceManager* _the_instance_manager;
}

void Romain::PThreadLock_priv::status() const
{
	INFO() << "[lock] MTX.lock    = " << mtx_lock_count;
	INFO() << "[lock] MTX.unlock  = " << mtx_unlock_count;
	INFO() << "[lock] pt.lock     = " << pt_lock_count;
	INFO() << "[lock] pt.unlock   = " << pt_unlock_count;
	INFO() << "[lock] # ignored   = " << ignore_count;
	INFO() << "[lock] Total count = " << total_count;
}


Romain::Observer::ObserverReturnVal
Romain::PThreadLock_priv::notify(Romain::App_instance* inst,
                                 Romain::App_thread*   thread,
                                 Romain::Thread_group* group,
                                 Romain::App_model*    model)
{
	if (!entry_reason_is_int3(thread->vcpu(), inst, model)) {
		return Romain::Observer::Ignored;
	}

	DEBUG() << group->name << " entering LOCK observer";
	l4util_inc32(&total_count);

#define HANDLE_BP(var, handler) do { \
		if(_functions[(var)].orig_address == thread->vcpu()->r()->ip - 1) { \
			(handler)(inst, thread, group, model); \
			return Romain::Observer::Replicatable; \
		} \
	} while (0)

	HANDLE_BP(mutex_lock_id, mutex_lock);
	HANDLE_BP(mutex_unlock_id, mutex_unlock);
	//HANDLE_BP(mutex_init_id, mutex_init);
	HANDLE_BP(pt_lock_id, lock);
	HANDLE_BP(pt_unlock_id, unlock);

#undef HANDLE_BP

	DEBUG() << "{" << group->name << "}" << " ignored lock...";
	l4util_inc32(&ignore_count);
	return Romain::Observer::Ignored;
}


void Romain::PThreadLock_priv::attach_lock_info_priv(Romain::App_instance*inst, Romain::App_model *am)
{

	L4::Cap<L4Re::Dataspace> page;
	l4_addr_t page_local  = Romain::Region_map::allocate_and_attach(&page, L4_PAGESIZE, 0, 0);
	
	inst->vcpu_task()->map(L4Re::This_task, l4_fpage((l4_addr_t)page_local, L4_PAGESHIFT, L4_FPAGE_RO),
						   PRIV_INFO_ADDR);
	((lip_priv_info*)page_local)->replica_id = inst->id();
}


void Romain::PThreadLock_priv::attach_lock_info_page(Romain::App_model *am)
{
	l4_size_t lip_size = l4_round_page(sizeof(lock_info));
	_lip_local = Romain::Region_map::allocate_and_attach(&_lip_ds, lip_size);

	INFO() << "Local LIP address: " << std::hex << _lip_local
	       << " size = " << lip_size << " target " << LOCK_INFO_ADDR;
	void* remote__lip = (void*)am->prog_attach_ds(LOCK_INFO_ADDR,
	                                              lip_size, _lip_ds, 0, 0,
	                                              "lock info page",
	                                              _lip_local, true);
	_check(reinterpret_cast<l4_umword_t>(remote__lip) != LOCK_INFO_ADDR, 
	       "LIP did not attach to proper remote location");

	INFO() << "Remotely attached LIP to " << remote__lip;

#if 1
	/*
	 * Reserve guard pages before and after the LIP. This will make sure that the
	 * application never attaches memory directly adjacent to the LIP. A stray write
	 * (e.g., a memcpy) from a valid application address will therefore never touch
	 * the LIP because it will beforehand raise a page fault in the guard pages.
	 *
	 * [This does not protect valid LIP writes, though. For this we use LIP canaries.]
	 */
	l4_addr_t remote_start = reinterpret_cast<l4_addr_t>(remote__lip);
	l4_addr_t remote_end   = remote_start + lip_size;

	_check(am->rm()->attach_area(remote_start - L4_PAGESIZE, L4_PAGESIZE) == L4_INVALID_ADDR,
		   "Could not reserve LIP guard #1");
	INFO() << "Reserved [" << std::hex << remote_start - L4_PAGESIZE << " - "
	       << remote_start << "]";

	_check(am->rm()->attach_area(remote_end, L4_PAGESIZE) == L4_INVALID_ADDR,
		   "Could not reserve LIP guard #2");
	INFO() << "Reserved [" << std::hex << remote_end << " - "
	       << remote_end + L4_PAGESIZE << "]";

	am->lockinfo_local(_lip_local);
	am->lockinfo_remote(LOCK_INFO_ADDR);
	//l4_touch_rw((void*)_lip_local, lip_size);
#endif

	memset((void*)_lip_local, 0, lip_size);

#if 1
	/* Initialize LIP canaries that will protect single LIP lock entries
	 * from being arbitrarily overwritten by stray writes witihin the LIP.
	 */
	lock_info *lip             = reinterpret_cast<lock_info*>(_lip_local);
	for (unsigned i = 0; i < NUM_LOCKS; ++i) {
		// arbitrary fixed canary value - we only want to protect against
		// erroneous overwrite, we don't assume malicious code
		lip->locks[i].canary = static_cast<l4_uint16_t>(LIP_CANARY);
	}
#endif
}


void Romain::PThreadLock_priv::startup_notify(Romain::App_instance *inst,
                                              Romain::App_thread *,
                                              Romain::Thread_group *,
                                              Romain::App_model *am)
{
	static l4_umword_t callCount  = 0;
	lock_info *lip             = reinterpret_cast<lock_info*>(_lip_local);

#if INTERNAL_DETERMINISM
	if (!callCount) {
		/*
		 * For internal determinism, we make sure that we only attach the
		 * lock info page once (for the first replica), because the LIP
		 * is shared across all replicas.
		 */
		attach_lock_info_page(am);
		lip                    = reinterpret_cast<lock_info*>(_lip_local);
		lip->locks[0].lockdesc = 0xFAFAFAFA;
		lip->locks[0].owner    = 0xDEADBEEF;
		_check(am->rm()->attach_area(PRIV_INFO_ADDR, L4_PAGESIZE) == L4_INVALID_ADDR,
		       "Could not reserve private lock info area");

#endif
		//DEBUG() << "Replica LIP address: " << std::hex << remote_lock_info;

		/*
		 * Breakpoints / patching of function entries is only done once,
		 * because code is shared across all replicas.
		 */
		for (l4_umword_t idx = 0; idx < pt_max_wrappers; ++idx) {
			//DEBUG() << idx;
			_functions[idx].activate(inst, am);
		}
#if INTERNAL_DETERMINISM
	}
	attach_lock_info_priv(inst, am);
	callCount++;
	lip->replica_count += 1;

	// TODO: Iterate over the LIP's pages and map them all
	//       immediately to prevent getting pagefaults while
	//       executing pthread_rep code
	l4_umword_t map_size = l4_round_page(sizeof(lock_info));
	l4_addr_t   src_addr = reinterpret_cast<l4_addr_t>(_lip_local);
	l4_addr_t   dst_addr = LOCK_INFO_ADDR;

	while (map_size > 0) {
		inst->map_aligned(src_addr, dst_addr, L4_PAGESHIFT, L4_FPAGE_RW);
		src_addr += L4_PAGESIZE;
		dst_addr += L4_PAGESIZE;
		map_size -= L4_PAGESIZE;
	}

#endif
	//enter_kdebug();
}


/*
 * pthread_mutex_init()
 *   - mutex address    @ ESP + 4
 *   - mutex attrib ptr @ ESP + 8
 *   - returns int in EAX
 */
void Romain::PThreadLock_priv::mutex_init(Romain::App_instance *inst,
                                          Romain::App_thread *t,
                                          Romain::Thread_group *tg,
                                          Romain::App_model *am)
{
	l4_addr_t stack  = am->rm()->remote_to_local(t->vcpu()->r()->sp, inst->id());
	l4_umword_t ret  = *(l4_umword_t*)stack;
	l4_umword_t lock = *(l4_umword_t*)(stack + 1*sizeof(l4_umword_t));
	l4_umword_t attr = *(l4_umword_t*)(stack + 2*sizeof(l4_umword_t));

	DEBUG() << "INIT: lock @ " << std::hex << lock << ", attr " << attr;
	if (attr != 0) {
		enter_kdebug("init with attributes");
	}

	_locks[lock] = new PThreadMutex(false /*XXX*/);

	t->vcpu()->r()->ax  = 0;
	t->return_to(ret);
}


/*
 * __pthread_lock():
 * 		- called with lock pointer in EAX
 * 		- returns void
 */
void Romain::PThreadLock_priv::lock(Romain::App_instance *inst,
                                    Romain::App_thread *t,
                                    Romain::Thread_group *tg,
                                    Romain::App_model *am)
{
#if BENCHMARKING
	unsigned long long t1, t2;
	t1 = l4_rdtsc();
#endif
	l4util_inc32(&pt_lock_count);
	l4_addr_t stack  = am->rm()->remote_to_local(t->vcpu()->r()->sp, inst->id());
	l4_umword_t ret  = *(l4_umword_t*)stack;
	l4_umword_t lock = t->vcpu()->r()->ax;

	EVENT(Measurements::LockEvent::lock, lock);

	DEBUG() << "{" << tg->name << "}" << "Stack ptr " << std::hex << t->vcpu()->r()->sp << " => "
         	<< stack;
	DEBUG() << "{" << tg->name << "}" << "Lock @ " << std::hex << lock;
	DEBUG() << "{" << tg->name << "}" << "Return addr " << std::hex << ret;

	lookup_or_create(lock)->lock(tg);

	t->return_to(ret);
#if BENCHMARKING
	t2 = l4_rdtsc();
	t->count_lock(t2-t1);
#endif
}


/*
 * __pthread_unlock():
 * 		- called with lock pointer as single stack argument
 * 		- returns int
 */
void Romain::PThreadLock_priv::unlock(Romain::App_instance *inst,
                                      Romain::App_thread *t,
                                      Romain::Thread_group *tg,
                                      Romain::App_model *am)
{
#if BENCHMARKING
	unsigned long long t1, t2;
	t1 = l4_rdtsc();
#endif
	l4util_inc32(&pt_unlock_count);
	l4_addr_t stack     = am->rm()->remote_to_local(t->vcpu()->r()->sp, inst->id());
	l4_umword_t retaddr = *(l4_umword_t*)stack;
	l4_umword_t lock    = *(l4_umword_t*)(stack + 1*sizeof(l4_umword_t));

	EVENT(Measurements::LockEvent::unlock, lock);

	DEBUG() << "{" << tg->name << "}" << "Return addr " << std::hex << retaddr;
	DEBUG() << "{" << tg->name << "}" << "Lock @ " << std::hex << lock;

	//enter_kdebug("unlock");

	int ret = lookup_or_fail(lock)->unlock();

	t->vcpu()->r()->ax  = ret;
	t->return_to(retaddr);
#if BENCHMARKING
	t2 = l4_rdtsc();
	t->count_lock(t2-t1);
#endif
}

/*
 * pthread_mutex_lock()
 * 		- mutex   @ ESP + 4
 * 		- returns int
 */
void Romain::PThreadLock_priv::mutex_lock(Romain::App_instance* inst, Romain::App_thread* t,
                                          Romain::Thread_group* group, Romain::App_model* model)
{
#if BENCHMARKING
	unsigned long long t1, t2;
	t1 = l4_rdtsc();
#endif
	l4util_inc32(&mtx_lock_count);

	l4_addr_t stack     = model->rm()->remote_to_local(t->vcpu()->r()->sp, inst->id());
	l4_umword_t retaddr = *(l4_umword_t*)stack;
	l4_umword_t lock    = *(l4_umword_t*)(stack + 1*sizeof(l4_umword_t));

	EVENT(Measurements::LockEvent::mtx_lock, lock);

	DEBUG() << "lock @ " << std::hex << lock << " ESP.local = " << stack;
	PThreadMutex* mtx = _locks[lock];
	if (!mtx) {
		/*
		 * Can't use the shortcut here. We found a yet unknown lock. Now we need to 
		 * figure out whether it should be recursive. The respective member in the
		 * pthread data structure is at offset 12 (dec) from the mutex pointer
		 * we received as the argument.
		 */
		l4_umword_t mtx_kind_ptr = model->rm()->remote_to_local(lock + 12, inst->id());
#if 0
		DEBUG() << "log kind: " << *(l4_umword_t*)mtx_kind_ptr
		        << " RECURSIVE: " << PTHREAD_MUTEX_RECURSIVE_NP;
#endif

		mtx              = new PThreadMutex(*(l4_umword_t*)mtx_kind_ptr == PTHREAD_MUTEX_RECURSIVE_NP);
		_locks[lock]     = mtx;
	}

	int ret = mtx->lock(group);

	t->vcpu()->r()->ax  = ret;
	t->return_to(retaddr);
#if BENCHMARKING
	t2 = l4_rdtsc();
	t->count_lock(t2-t1);
#endif
}


void Romain::PThreadLock_priv::mutex_unlock(Romain::App_instance* inst, Romain::App_thread* t,
                                            Romain::Thread_group* group, Romain::App_model* model)
{
#if BENCHMARKING
	unsigned long long t1, t2;
	t1 = l4_rdtsc();
#endif
	l4util_inc32(&mtx_unlock_count);

	l4_addr_t stack     = model->rm()->remote_to_local(t->vcpu()->r()->sp, inst->id());
	l4_umword_t retaddr = *(l4_umword_t*)stack;
	l4_umword_t lock    = *(l4_umword_t*)(stack + 1*sizeof(l4_umword_t));

	EVENT(Measurements::LockEvent::mtx_unlock, lock);

	int ret = lookup_or_fail(lock)->unlock();
	DEBUG() << "unlock @ " << std::hex << lock << " = " << ret;

	t->vcpu()->r()->ax  = ret;
	t->return_to(retaddr);
#if BENCHMARKING
	t2 = l4_rdtsc();
	t->count_lock(t2-t1);
#endif
}

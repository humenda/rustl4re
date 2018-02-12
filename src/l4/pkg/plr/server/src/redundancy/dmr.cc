/*
 * dmr.cc --
 *
 *    n-way modular redundancy implementation 
 *
 * (c) 2011-2013 Björn Döbel <doebel@os.inf.tu-dresden.de>,
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include "../log"
#include "../redundancy.h"
#include "../app_loading"
#include "../fault_observers"
#include "../manager"
#include "../fault_handlers/syscalls_handler.h"
#include "../fault_handlers/debugging.h"

#include <l4/re/env>

#define MSG() DEBUGf(Romain::Log::Redundancy)
#define MSGi(inst) MSG() << "[" << (inst)->id() << "] "

//extern char * __func__;

/* Replication protocol:
 * =====================
 *
 * Everyone goes to sleep, except the last thread to enter. This thread becomes
 * the 'leader'. The leader returns from this function with the First_syscall return
 * value. It then goes on to execute the system call (in manager.cc). Depending on
 * its return value,
 *
 * a) For replicatable calls: it stores its VCPU state after the system call using
 *    the function put(). All other replicas then use get() to obtain this state.
 *
 * b) For non-replicatable calls: it sets the other replicas' return value to 
 *    Repeat_syscall. The replicas then perform handling themselves.
 *
 * After all the handling, everyone waits in resume() until the last replica reaches
 * the resumption point. Then each VCPU goes back to where it came from.
 *
 *
 * Detection and recovery:
 * =======================
 *
 * Before executing the fault handler, the leader checksums all VCPU states. If a
 * mismatch is found, it calls the recover() function. recover() sets things straight
 * so that after the handler is done, everyone is in an identical state again. The leader
 * then goes on to execute the call.
 */

Romain::DMR::DMR(l4_umword_t instances)
    : _enter_count(0),
#if WATCHDOG
     _watchdog_count(0),
#endif
      _leave_count(0), _block_count(0),
	  ts_lead(0),
      _rv(Romain::RedundancyCallback::Invalid),
      _num_instances(instances), _num_instances_bak(0)
#if WATCHDOG
      ,
      _got_watchdog(false), _got_other_trap(false), _other_trap_chance(false)
#endif
{
	for (l4_umword_t i = 0; i < _num_instances; ++i)
		_orig_vcpu[i] = 0;
	_check(pthread_mutex_init(&_enter_mtx, NULL) != 0, "error initializing mtx");
	_check(pthread_cond_init(&_enter, NULL) != 0,      "error initializing condvar");
	_check(pthread_mutex_init(&_leave_mtx, NULL) != 0, "error initializing mtx");
	_check(pthread_cond_init(&_leave, NULL) != 0,      "error initializing condvar");
	_check(pthread_mutex_init(&_block_mtx, NULL) != 0, "error initializing mtx");
	_check(pthread_cond_init(&_block, NULL) != 0,      "error initializing condvar");
#if WATCHDOG
	_check(pthread_mutex_init(&_watchdog_mtx, NULL) != 0, "error initializing mtx");
	_check(pthread_cond_init(&_watchdog_cond, NULL) != 0, "error initializing condvar");
#endif
}


void
Romain::Replicator::put(Romain::App_thread *t)
{
	//memset(&_regs, 0, sizeof(_regs)); // XXX
#define PUT(field) _regs.field = t->vcpu()->r()->field
	PUT(es); PUT(ds); PUT(gs); PUT(fs);
	PUT(di); PUT(si); PUT(bp); PUT(pfa);
	PUT(ax); PUT(bx); PUT(cx); PUT(dx);
	PUT(trapno); PUT(err); PUT(ip); PUT(flags);
	PUT(sp); PUT(ss);
#undef PUT
	l4_utcb_t *addr = reinterpret_cast<l4_utcb_t*>(t->remote_utcb());
	memcpy(&_utcb, addr, L4_UTCB_OFFSET);
}


void
Romain::Replicator::get(Romain::App_thread *t)
{
#define PUT(field) t->vcpu()->r()->field = _regs.field
	PUT(es); PUT(ds); PUT(gs); PUT(fs);
	PUT(di); PUT(si); PUT(bp); PUT(pfa);
	PUT(ax); PUT(bx); PUT(cx); PUT(dx);
	PUT(trapno); PUT(err); PUT(ip); PUT(flags);
	PUT(sp); PUT(ss);
#undef PUT
	l4_utcb_t *addr = reinterpret_cast<l4_utcb_t*>(t->remote_utcb());
	memcpy(addr, &_utcb, L4_UTCB_OFFSET);
}

bool
Romain::DMR::checksum_replicas(Romain::App_instance *i, Romain::Thread_group *tg, Romain::App_thread *t)
{
	l4_umword_t csums[MAX_REPLICAS] = {0, };
	l4_umword_t idx;

	// calc checksums
	for (idx = 0; idx < _num_instances; ++idx) {
		if (_orig_vcpu[idx] < (App_thread*)0x1000) {
			ERROR() << "My ID: " << i->id();
			ERROR() << std::hex << "ID " << idx << " vcpu ptr " << _orig_vcpu[idx];
			ERROR() << std::dec << "leave ct " << _leave_count << " enter ct " << _enter_count;
		}
		csums[idx] = _orig_vcpu[idx]->csum_state();
	}

	// validate checksums
	for (idx = 1; idx < _num_instances; ++idx)
		if (csums[idx] != csums[idx-1]) {
#if 1
			ERROR() << BOLD_RED << "State mismatch detected!";
			ERROR() << "=== vCPU states ===";
			for (l4_umword_t cnt = 0; cnt < _num_instances; ++cnt) {
				ERROR() << "--- instance " << cnt << " @ "
					<< _orig_vcpu[cnt]->vcpu() << " (cs: "
					<< std::hex << csums[cnt] << ") ---";
				if (_orig_vcpu[cnt]) {
					_orig_vcpu[cnt]->vcpu()->print_state();
					ERROR() << "vcpu.err = " << std::hex << _orig_vcpu[cnt]->vcpu()->r()->err << "\n";
				}
			}
			ERROR() << "Instances: " << _num_instances << " this inst " << idx << "\n";
#endif
			return false;
		}

	return true;
}

class RecoverAbort
{
	public:
		static __attribute__((noreturn)) void recover()
		{
			static bool used = false;

			if (!used){
				used = true;
				ERROR() << "Aborting after error.\n";
				Romain::_the_instance_manager->show_stats();
				throw("ERROR -> abort");
			}

			exit(12);
		}
};


class RedundancyAbort
{
	public:
		static void recover(Romain::App_thread** threads, l4_umword_t count,
		                    l4_umword_t *good, l4_umword_t *bad)
		{
			l4_umword_t csums[count];
			l4_umword_t idx;

			// calc checksums
			for (idx = 0; idx < count; ++idx)
				csums[idx] = threads[idx]->csum_state();

			// find mismatch
			for (idx = 1; idx < count; ++idx)
				if (csums[idx] != csums[idx-1]) { // mismatch
					if (csums[idx] == csums[(idx + 1) % count]) {
						*good = idx;
						*bad  = idx-1;
					} else {
						*good = idx-1;
						*bad  = idx;
					}
				}
		}
};


void
Romain::DMR::recover(Romain::App_model *am)
{
	if (_num_instances < 3)
		RecoverAbort::recover(); // noreturn

	l4_umword_t good = ~0, bad = ~0;
	RedundancyAbort::recover(_orig_vcpu, _num_instances, &good, &bad);
	DEBUG() << "good " << good << ", bad " << bad;

	// XXX: This does not suffice. We also need to copy memory content
	//      from a correct replica to the incorrect one
	replicator().put(_orig_vcpu[good]);
	replicator().get(_orig_vcpu[bad]);
	am->rm()->replicate(good, bad);

#if 0
	DEBUG() << "after recovery:";
	for (l4_umword_t i = 0; i < _num_instances; ++i)
		DEBUG() << i << " " << std::hex << _orig_vcpu[i]->csum_state();
#endif
}

#if WATCHDOG
void
Romain::DMR::watchdog_prepare(Romain::App_model *a)
{
	App_thread *leader;
	bool skip = false;
	bool watchdog_sync = true;
	
	// we presume that the largest ip means wider progress
	leader = _orig_vcpu[0];
	for (unsigned i = 0; i < _num_instances; i++) {
		if (_orig_vcpu[i]->vcpu()->r()->ip > leader->vcpu()->r()->ip)
			leader = _orig_vcpu[i];
	}

	if (_got_other_trap) {
		if (!_other_trap_chance) {
			_other_trap_chance = true;
			DEBUG() << "Got other trap, resume others and try to catch up...";
			watchdog_sync = false;
		} else {
			DEBUG() << "Other replicas didn't catch up to replica with another trap";
			_other_trap_chance = false;
			unsigned watchdog_count = 0;
			unsigned other_trap_count = 0;
			for (unsigned i = 0; i < _num_instances; i++) {
				if (_orig_vcpu[i]->got_watchdog()) {
					watchdog_count++;
					DEBUG() << "got watchdog, ip = 0x" << std::hex << _orig_vcpu[i]->vcpu()->r()->ip;
				}
				if (_orig_vcpu[i]->got_other_trap()) {
					other_trap_count++;
					DEBUG() << "got other trap, ip = 0x" << std::hex << _orig_vcpu[i]->vcpu()->r()->ip;
				}
			}
			if (watchdog_count == _num_instances) {
				watchdog_sync = true;
				_got_watchdog = true;
				_got_other_trap = false;
			} else if (other_trap_count == _num_instances) {
				_got_watchdog = false;
				_got_other_trap = true;
				watchdog_sync = false;
				if (!checksum_replicas())
					recover(a);
			} else {
				if (watchdog_count > other_trap_count) {
					for (unsigned i = 0; i < _num_instances; i++) {
						if (_orig_vcpu[i]->got_other_trap()) {
							DEBUG() << "Majority desicion: suspend replica " << i << " that got the other trap";
							_orig_vcpu[i]->watchdog_suspended(true);
						} else {
							leader = _orig_vcpu[i];
						}
					}
					for (unsigned i = 0; i < _num_instances; i++) {
						if (!_orig_vcpu[i]->watchdog_suspended())
							if (_orig_vcpu[i]->vcpu()->r()->ip > leader->vcpu()->r()->ip)
								leader = _orig_vcpu[i];
					}
				} else if (watchdog_count <= other_trap_count) {
					DEBUG() << "Majority desicion: Trying to recover the replica that got the watchdog interrupt";
					watchdog_sync = false;
					_got_watchdog = false;
					if (!checksum_replicas())
						recover(a);
				}
			}
		}
	}

	if (watchdog_sync) {
		if (leader->vcpu()->r()->ip >= (l4_addr_t)l4re_kip())
			skip = true;

		if (skip)
			DEBUG() << "Watchdog: Skipping sync...";
		else {
			if (_got_watchdog && !_got_other_trap)
				DEBUG() << "Watchdog: First sync attempt with leader ip = 0x" 
					<< std::hex << leader->vcpu()->r()->ip;
		}

		_watchdog->skip(skip);
		_watchdog->passed(false);
		_watchdog->retry(false);
		_watchdog->retry_count(0);

		_watchdog->leader(leader);
		_watchdog->old_leader(leader);
	}
}
#endif

Romain::RedundancyCallback::EnterReturnVal
Romain::DMR::enter(Romain::App_instance *i, Romain::App_thread *t,
                   Romain::Thread_group* tg, Romain::App_model *a)
{
	(void)a;

	MSGi(i) << "{" << tg->name << "}" << "DMR::enter act(" << _enter_count << ", " << _leave_count << ")";
	int ctr = 0;
	while (_leave_count.load() != 0) {
		/* Basic, hacky watchdog implementation. We wait here until the last replica has
		 * exited its previous fault handler. This should happen fast, because if we reached
		 * this point, this means:
		 *
		 *    a) we were released from the previous handler and already hit this place again
		 *    b) everyone else should be released from the prev. handler and soon arrive here
		 */
		++ctr;
		if (ctr > 1000000) {
			ctr = 0;
			INFO() << "{" << tg->name << "." << i->id() << "}"
			       << " might be stuck. enter count " << _enter_count.load()
			       << " leave count: " << _leave_count.load();
#if 0
			for (auto thread : tg->threads) {
				INFO() << "{" << tg->name << "." << i->id() << "}"
				       << "    Thread: " << thread->getenter()
				       << " " << thread->getleave();
			}
#endif
		}
		l4_thread_yield();
	}

	Romain::RedundancyCallback::EnterReturnVal ret = Romain::RedundancyCallback::First_syscall;

	// enter ourselves into the list of faulted threads
	if (!t) {
		ERROR() << std::hex << t;
	}
	_orig_vcpu[i->id()] = t;

#if WATCHDOG
	if (_watchdog->enabled()) {
		if (t->vcpu()->is_irq_entry() && t->vcpu()->i()->label == Romain::Watchdog_irq_label) {
			_got_watchdog = true;
			t->got_watchdog(true);
			t->got_other_trap(false);
		} else {
			_got_other_trap = true;
			t->got_other_trap(true);
			t->got_watchdog(false);
		}
	}
#endif

	/* TODO: select the first replica that makes the sum of all replicas
	 *       larger than N/2, if all their states match.
	 */
	int ec = _enter_count.fetch_add(1);
	if (ec != (_num_instances-1)) {
		MSGi(i) << "{" << tg->name << "} " << "I'm not the last instance " << ec << " -> going to wait.";
		// wait for the leader

#if BENCHMARKING
		unsigned long long t1 = l4_rdtsc();
#endif
#if DMR_SYNC_SHM
		while (_leave_count == 0) {
			l4_thread_yield();
		}
#else
		pthread_mutex_lock(&_enter_mtx);
		if (_leave_count == 0) {
		pthread_cond_wait(&_enter, &_enter_mtx);
		}
		pthread_mutex_unlock(&_enter_mtx);
#endif
#if BENCHMARKING
		unsigned long long t2 = l4_rdtsc();
		t->inc_wait(t2 - t1);
#endif
		// get the return value set by the leader
#if WATCHDOG
		if (!_watchdog->enabled() || (_got_other_trap && !_got_watchdog)) {
			ret = _rv;
		}
#else
		ret = _rv;
#endif
	} else {
#if BENCHMARKING
		ts_lead = l4_rdtsc();
#endif
		t->activate();
		//tsc = l4_rdtsc() - tsc;
		// qsort: <= 0x2000
		// susan: <= 3000
		// basic math: <= 13000
#if WATCHDOG
		if (!_watchdog->enabled() || (_got_other_trap && !_got_watchdog)) {
		// everyone is here, so checksum the VCPUs now
			if (!checksum_replicas()) {
			recover(a);
			}
		// at this point, recovery has made sure that all replicas
		// are in the same state.
		} else {
			if (_got_watchdog) {
				watchdog_prepare(a);
				pthread_cond_broadcast(&_enter);
			}
		}
#else
		if (!checksum_replicas(i,tg,t)) {
			recover(a);
		}
		_enter_count.store(0);
#endif
	}


#if WATCHDOG
	if (_watchdog->enabled() && _got_watchdog) {
		pthread_mutex_lock(&_watchdog_mtx);
		if (++_watchdog_count < _num_instances) {
			pthread_cond_wait(&_watchdog_cond, &_watchdog_mtx);
		} else {
			_help_got_other_trap = _got_other_trap;
			_help_got_watchdog = _got_watchdog;
			_got_other_trap = false;
			_got_watchdog = false;
			_watchdog_count = 0;
			pthread_cond_broadcast(&_watchdog_cond);
		}
		pthread_mutex_unlock(&_watchdog_mtx);
		
		if (_help_got_watchdog && !_help_got_other_trap) {
			/* all replicas got the watchdog interrupt */
			t->got_watchdog(false);
			ret = _watchdog->enter(i, t, a);
			if (ret == Romain::RedundancyCallback::Watchdog_passed)
				ret = enter(i, t,tg,  a);
		} else if (_help_got_watchdog && _help_got_other_trap) {
			/* some got another trap and some the watchdog interrupt
			 * so resume the replicas that got the watchdog interrupt because they will encounter
			 * the same trap like the others (if no errorneous replicas are present)
			 */
			if (t->got_watchdog()) {
				L4::Cap<L4::Thread> vcpu_cap = t->vcpu_cap();
				t->got_watchdog(false);

				if (t->watchdog_timeout() < Romain::Watchdog::SynchronizationTimeout) {
					_watchdog->reset(i, t, t->watchdog_timeout());
				} else {
					if (_watchdog->passed() && _watchdog->retry_count() > 0)
						_watchdog->reset(i, t, _watchdog->retry_count()*Romain::Watchdog::SynchronizationTimeout);
					else
						_watchdog->reset(i, t, Romain::Watchdog::SynchronizationTimeout);
				}

				vcpu_cap->vcpu_resume_commit(vcpu_cap->vcpu_resume_start());
			} else {
				/* we got another trap so re-block until others are at the same point 
				 * or the watchdog timeout hits for them
				 */
			  t->got_other_trap(false);
				ret = enter(i, t, tg, a);
			}
		}
	} else {
		t->got_other_trap(false);
	}
#endif

	/*
	 * If the leader told us to skip the syscall, get replicated VCPU and
	 * UTCB states here.
	 */
	if (ret == Romain::RedundancyCallback::Skip_syscall) {
#if BENCHMARKING
		unsigned long long t1 = l4_rdtsc();
#endif
		replicator().get(t);
#if BENCHMARKING
		t->inc_getdata(l4_rdtsc()-t1);
#endif
	}

	return ret;
}


void Romain::DMR::leader_repeat(Romain::App_instance *i, Romain::App_thread *t,
                                Romain::App_model *a)
{
	(void)i; (void)t; (void)a;
	MSGi(i) << __func__;
	_rv = Romain::RedundancyCallback::Repeat_syscall;
}


void Romain::DMR::leader_replicate(Romain::App_instance *i, Romain::App_thread *t,
                                   Romain::App_model *a)
{
	(void)i; (void)t; (void)a;
	MSGi(i) << __func__;
	_rv = Romain::RedundancyCallback::Skip_syscall;

	//t->print_vcpu_state();
	replicator().put(t);
}


void Romain::DMR::resume(Romain::App_instance *i, Romain::App_thread *t,
                         Romain::Thread_group* tg, Romain::App_model *a)
{
	(void)i; (void)t; (void)a;

	/* We are still the only one executing here... */
	if (_leave_count == 0) {
		for (l4_umword_t i = 0; i < _num_instances; ++i)
			_orig_vcpu[i] = 0;
	}

	/*
	 * In the DMR_SYNC_SHM case this will trigger the other
	 * replicas to continue. If !DMR_SYNC_SHM we still need
	 * to send some notifications.
	 */
	unsigned int lc = _leave_count.fetch_add(1);
	MSGi(i) << "{" << tg->name << "}" << "Leave count: " << lc;
#if !DMR_SYNC_SHM
	if (lc == 0) {
		pthread_mutex_lock(&_enter_mtx);
		pthread_cond_broadcast(&_enter);
		pthread_mutex_unlock(&_enter_mtx);
	}
#endif
	
	// Yes, this branch may be taken even if we took the previous
	// one: this would be the single-replica case then!
	if (lc == (_num_instances-1)) {
		_leave_count.store(0);
	}

#if 0
	//MSGi(i) << "[l] acquiring leave mtx";
	pthread_mutex_lock(&_leave_mtx);
	if (_leave_count == 0) {
		pthread_mutex_lock(&_enter_mtx);
		pthread_cond_broadcast(&_enter);
		pthread_mutex_unlock(&_enter_mtx);
	}

	//MSGi(i) << "++_leave_count " << _leave_count;
	if (++_leave_count < _num_instances) {
		MSGi(i) << "Waiting for other replicas to commit their syscall.";
		//MSGi(i) << "cond_wait(leave)";
		pthread_cond_wait(&_leave, &_leave_mtx);
		//MSGi(i) << "success: cond_wait(leave)";
	} else {
		for (l4_umword_t i = 0; i < _num_instances; ++i)
			_orig_vcpu[i] = 0;
		pthread_cond_broadcast(&_leave);
	}
	//MSGi(i) << "counts @ resume: " << _enter_count << " " << _leave_count;
	--_leave_count;
	pthread_mutex_unlock(&_leave_mtx);

#endif
}

void Romain::DMR::wait(Romain::App_instance *i, Romain::App_thread *t,
                       Romain::App_model *a)
{
	MSGi(i) << __func__;
	pthread_mutex_lock(&_block_mtx);
	++_block_count;
	MSGi(i) << "going to wait. block_count: " << _block_count;
	pthread_cond_broadcast(&_enter);
	pthread_cond_wait(&_block, &_block_mtx);
	pthread_mutex_unlock(&_block_mtx);
}

void Romain::DMR::silence(Romain::App_instance *i, Romain::App_thread *t,
                          Romain::App_model *a)
{
	MSGi(i) << __func__;
	// 1. Tell anyone who is still waiting to enter that he can now do so.
	//    These replicas will all run until they block on _block_mtx.
	pthread_cond_broadcast(&_enter);

	while (_block_count < (_num_instances - 1))
		l4_sleep(20); // XXX handshake

	_num_instances_bak = _num_instances;
	_num_instances     = 1;
}

void Romain::DMR::wakeup(Romain::App_instance *i, Romain::App_thread *t,
                         Romain::App_model *a)
{
	MSGi(i) << __func__;
	_block_count   = 0;
	_num_instances = _num_instances_bak;
	pthread_cond_broadcast(&_block);
}

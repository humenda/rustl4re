// vim: ft=cpp

/*
 * watchdog.cc --
 *
 *     Implementation of an instruction-based watchdog
 *
 * (c) 2013 Martin Kriegel <mkriegel@os.inf.tu-dresden.de>,
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include "watchdog.h"
#include "fault_handlers/debugging.h"
#include <l4/util/rdtsc.h>

Breakpoint *break_point = 0;

bool
Romain::Watchdog::enable(App_instance *i, App_thread *t)
{
	bool ret = true;
	char tmp[25];
	l4_msgtag_t tag;

	if (!_enabled) {
		pthread_mutex_lock(&_help_mtx);
		_enabled = true;
		pthread_mutex_unlock(&_help_mtx);
	}

	snprintf(tmp, 25, "Watchdog irq cap alloc %i", i->id());
	t->watchdog_irq(chkcap(L4Re::Util::cap_alloc.alloc<L4::Irq>(), tmp));
	
	snprintf(tmp, 25, "watchdog irq %i", i->id());
	chksys(L4Re::Env::env()->factory()->create(t->watchdog_irq()), tmp);
	
	l4_debugger_set_object_name(t->watchdog_irq().cap(), tmp);

	chksys(t->watchdog_irq()->attach(Romain::Watchdog_irq_label, t->vcpu_cap()));

	l4_utcb_mr()->mr[3] = -(t->watchdog_timeout());
	tag = l4_thread_watchdog_enable(t->vcpu_cap().cap(), t->watchdog_irq().cap());
	
	if (l4_msgtag_has_error(tag)) {
		pthread_mutex_lock(&_help_mtx);
		_enabled = false;
		pthread_mutex_unlock(&_help_mtx);
		ret = false;
	}

	return ret;
}

bool
Romain::Watchdog::disable(App_instance *i, App_thread *t)
{
	bool ret = true;
	l4_msgtag_t tag;

	DEBUG() << "Disabling watchdog for replica " << i->id();

	if (_enabled) {
		pthread_mutex_lock(&_help_mtx);
		_enabled = false;
		pthread_mutex_unlock(&_help_mtx);
	}

	l4_utcb_mr()->mr[1] = DisableSyscallNr;
	tag = l4_thread_watchdog_control(t->vcpu_cap().cap());

	if (l4_msgtag_has_error(tag))
		ret = false;

	return ret;
}

bool
Romain::Watchdog::disable_all()
{
	bool ret = true;
	l4_msgtag_t tag;


	DEBUG() << "Disabling watchdog for all replicas.";

	if (_enabled) {
		pthread_mutex_lock(&_help_mtx);
		_enabled = false;
		pthread_mutex_unlock(&_help_mtx);
	}

	for (int i = 0; i < _num_instances; i++) {
		l4_utcb_mr()->mr[1] = DisableSyscallNr;
		tag = l4_thread_watchdog_control(_orig_vcpu[i]->vcpu_cap().cap());
		if (l4_msgtag_has_error(tag))
			ret = false;
	}

	return ret;
}

bool
Romain::Watchdog::reset(App_instance *i, App_thread *t, l4_umword_t timeout)
{
	bool ret = true;
	l4_msgtag_t tag;

	DEBUG() << "Resetting watchog for replica " << i->id();

	l4_utcb_mr()->mr[1] = ResetSyscallNr;
	l4_utcb_mr()->mr[2] = -(timeout);
	tag = l4_thread_watchdog_control(t->vcpu_cap().cap());

	if (l4_msgtag_has_error(tag))
		ret = false;

	return ret;
}

bool
Romain::Watchdog::stop(App_instance *i, App_thread *t)
{
	bool ret = true;
	l4_msgtag_t tag;

	DEBUG() << "Stopping watchdog for replica " << i->id();

	l4_utcb_mr()->mr[1] = StopSyscallNr;
	tag = l4_thread_watchdog_control(t->vcpu_cap().cap());

	if (l4_msgtag_has_error(tag))
		ret = false;

	return ret;
}

bool
Romain::Watchdog::start(App_instance *i, App_thread *t)
{
	bool ret = true;
	l4_msgtag_t tag;

	DEBUG() << "Starting watchdog for replica " << i->id();

	l4_utcb_mr()->mr[1] = StartSyscallNr;
	tag = l4_thread_watchdog_control(t->vcpu_cap().cap());

	if (l4_msgtag_has_error(tag))
		ret = false;

 	return ret;
}

Romain::Watchdog::ChecksumReplicasRet
Romain::Watchdog::checksum_replicas()
{
	Romain::Watchdog::ChecksumReplicasRet ret;

	unsigned long csums[MAX_REPLICAS] = {0, };
	unsigned comparison_count 				= 0;
	unsigned equal_count 							= 0;

	for (unsigned i = 0; i < _num_instances; i++)
		csums[i] = _orig_vcpu[i]->csum_state();

	for (unsigned i = 0; i < _num_instances; i++) {
		for (unsigned j = 0; j < _num_instances; j++) {
			if (j > i) {
				comparison_count++;
				if (csums[i] == csums[j] && _orig_vcpu[i]->vcpu()->r()->ip == _orig_vcpu[j]->vcpu()->r()->ip)
					equal_count++;
			}
		}
	}

	if (comparison_count == equal_count) {
		ret = Romain::Watchdog::AllStatesEqual;
	} else {
		if (equal_count == 0) {
			ret = Romain::Watchdog::AllStatesDiffer;
		} else if ((equal_count >= _num_instances) || (equal_count > (_num_instances - equal_count))) {
			ret = Romain::Watchdog::MajorityStatesEqual;
		} else {
			ret = Romain::Watchdog::MinorityStatesEqual;
		}
	}

	return ret;
}

Romain::RedundancyCallback::EnterReturnVal
Romain::Watchdog::enter(Romain::App_instance *i, Romain::App_thread *t, Romain::App_model *a)
{
	Romain::RedundancyCallback::EnterReturnVal ret = Romain::RedundancyCallback::Invalid;

	_orig_vcpu[i->id()] = t;

	switch (_mode) {
		case Breakpointing:
			ret = breakpointing(i, t, a);
			break;
		case SingleStepping:
			ret = single_stepping(i, t, a);
			break;
		default:
			break;
	}

	return ret;
}

void
Romain::Watchdog::post_verification(Romain::App_model *a)
{
	bool give_up = false;
	
	if (!_passed) {
		switch (checksum_replicas()) {
			case AllStatesEqual:
				_retry = false;
				DEBUG() << "All replicas in the same state, continue...";
				break;
			case MajorityStatesEqual:
				_retry = false;
				DEBUG() << "A majority of replicas in the same state, recover...";
				_redundancyCB->recover(a);
				break;
			case MinorityStatesEqual:
				_retry = true;
				_skip = false;
				DEBUG() << "A minority of replicas in the same state, retry...";
				break;
			case AllStatesDiffer:
				_retry = true;
				_skip = false;
				DEBUG() << "All replica states differ, retry...";
				break;
			default:
				break;	
		}
	}
				
	if (_retry) {
		++_retry_count;
		if (_retry_count == _num_instances) {
			for (unsigned i = 0; i < _num_instances; i++) {
				if (!_orig_vcpu[i]->i_have_met_the_leader()) {
					_orig_vcpu[i]->watchdog_suspended(true);
					DEBUG() << "Not syncable: suspending replica " << i << " and retry...";
				}
			}
		} else if (_retry_count >= (2 * _num_instances)) {
			give_up = true;
			_retry = false;
		}
	}
	
	if (_retry) {
		App_thread *new_leader;
		for (unsigned i = 0; i < _num_instances; i++) {
			if (_orig_vcpu[i] != _leader && _orig_vcpu[i] != _old_leader
					&& !_orig_vcpu[i]->watchdog_suspended()) {
				new_leader = _orig_vcpu[i];
				break;				
			}
		}
		for (unsigned i = 0; i < _num_instances; i++) {
			if (_orig_vcpu[i] != _leader && _orig_vcpu[i] != _old_leader
					&& !_orig_vcpu[i]->watchdog_suspended()) {
				if (_orig_vcpu[i] != new_leader) {
					if (_orig_vcpu[i]->vcpu()->r()->ip > new_leader->vcpu()->r()->ip)
						new_leader = _orig_vcpu[i];
				}
			}	
		}
		_old_leader = _leader;
		_leader = new_leader;
		if (_leader->vcpu()->r()->ip >= (l4_addr_t)l4re_kip()) {
			_skip = true;
			_retry = false;
		}
	}

	if (_retry) {
		if (!give_up && !_skip) {
			DEBUG() << "Sync retry #" << _retry_count 
				<< " with new leader: ip = 0x" << std::hex << _leader->vcpu()->r()->ip;
		} else {
			DEBUG() << "Shortly before a system call, skipping sync...";
		}
	}
}

Romain::RedundancyCallback::EnterReturnVal
Romain::Watchdog::breakpointing(Romain::App_instance *i, Romain::App_thread *t, Romain::App_model *a)
{
	L4::Cap<L4::Thread> vcpu_cap = t->vcpu_cap();

	if (_skip) {
		reset(i, t, 2 * SynchronizationTimeout);
		vcpu_cap->vcpu_resume_commit(vcpu_cap->vcpu_resume_start());
	}

	if (t->watchdog_breakpointing() && t->vcpu()->r()->trapno == 0x3) {
		pthread_mutex_lock(&_bp_mtx);
		if (t->watchdog_breakpoint()->was_hit(t)) {
			t->vcpu()->r()->ip -= 1;
		} else {
			pthread_mutex_lock(&_bp_mtx);
			_passed = true;
			_retry = false;
			pthread_mutex_unlock(&_bp_mtx);
			t->its_me_who_passed_the_watchdog(true);
		}
		pthread_mutex_unlock(&_bp_mtx);
	}

	if (!t->watchdog_suspended() && !_passed && !t->watchdog_breakpointing()
			&& (t->vcpu()->r()->ip != _leader->vcpu()->r()->ip)) {
		DEBUG() << "[" << i->id() << "] Trying to catch up to leader from ip = 0x" << std::hex << t->vcpu()->r()->ip;
		
		t->watchdog_breakpointing(true);

		pthread_mutex_lock(&_bp_mtx);
		if (break_point == 0) {
			break_point = new Breakpoint(_leader->vcpu()->r()->ip);
			break_point->activate(i, a);
		}
		pthread_mutex_unlock(&_bp_mtx);
		
		t->watchdog_breakpoint(break_point);

		if (t->watchdog_timeout() < SynchronizationTimeout) {
			if (_retry && _retry_count > 0 && t == _old_leader) {
				reset(i, t, 2 * t->watchdog_timeout());
			} else {
				reset(i, t, t->watchdog_timeout());
			}
		} else {	
			if (_retry && _retry_count > 0 && t == _old_leader) {
				reset(i, t, 2 * SynchronizationTimeout);	
			}	else {
				reset(i, t, SynchronizationTimeout);
			}
		}
		vcpu_cap->vcpu_resume_commit(vcpu_cap->vcpu_resume_start());
	}

	if (t->watchdog_breakpointing()) {
		if (t->vcpu()->r()->trapno != 0x3 
			&& !(t->vcpu()->is_irq_entry() && t->vcpu()->i()->label == Romain::Watchdog_irq_label)) {
			pthread_mutex_lock(&_bp_mtx);
			_passed = true;
			_retry = false;
			pthread_mutex_unlock(&_bp_mtx);
			t->its_me_who_passed_the_watchdog(true);
		}

		t->watchdog_breakpointing(false);
	}

	if (t != _leader && t->vcpu()->r()->ip == _leader->vcpu()->r()->ip)
		t->i_have_met_the_leader(true);

	return leave(i, t, a);
}

Romain::RedundancyCallback::EnterReturnVal
Romain::Watchdog::single_stepping(Romain::App_instance *i, Romain::App_thread *t, Romain::App_model *a)
{
  L4::Cap<L4::Thread> vcpu_cap = t->vcpu_cap();

	if (_skip) {
		reset(i, t, 2 * SynchronizationTimeout);	
    vcpu_cap->vcpu_resume_commit(vcpu_cap->vcpu_resume_start());
  }

	if (t->watchdog_ss() && !t->watchdog_suspended()) {

		if (t->vcpu()->r()->ip >= (l4_addr_t)l4re_kip()) {
			pthread_mutex_lock(&_bp_mtx);
			_passed = true;
			pthread_mutex_unlock(&_bp_mtx);
		} else if (t->vcpu()->r()->trapno != 0x1) {
			pthread_mutex_lock(&_bp_mtx);
			_passed = true;
			pthread_mutex_unlock(&_bp_mtx);
			t->its_me_who_passed_the_watchdog(true);
		}
	}

	if (!_passed && !t->watchdog_suspended()) {
		if (t->vcpu()->r()->ip != _leader->vcpu()->r()->ip) {
			if (!t->watchdog_ss()) {
				DEBUG() << "[" << i->id() << "] Trying to catch up to leader from ip = 0x" << std::hex << t->vcpu()->r()->ip;
				t->watchdog_ss(true);
				t->vcpu()->r()->flags |= TrapFlag;
			}
			t->increment_watchdog_ss_count();
			if (_retry_count > 0 && t == _old_leader) {
				if (!(t->watchdog_ss_count() >= 2 * SynchronizationTimeout))
					vcpu_cap->vcpu_resume_commit(vcpu_cap->vcpu_resume_start());
			} else {
				if (!(t->watchdog_ss_count() >= SynchronizationTimeout))
					vcpu_cap->vcpu_resume_commit(vcpu_cap->vcpu_resume_start());
			}	
		}
	}
	
	if (t->watchdog_ss()) {
		/*
		if (!(_retry_count > 0 && t == _old_leader)) {
			DEBUG() << "Watchdog: needed " << t->watchdog_ss_count() << " single-steps";
			enter_kdebug("");
		}
		*/
		t->watchdog_ss(false);
		t->reset_watchdog_ss_count();
		t->vcpu()->r()->flags &= ~TrapFlag;
	}

	return leave(i, t, a);
}

Romain::RedundancyCallback::EnterReturnVal
Romain::Watchdog::leave(Romain::App_instance *i, Romain::App_thread *t, Romain::App_model *a)
{
	Romain::RedundancyCallback::EnterReturnVal ret = Romain::RedundancyCallback::Watchdog;

	pthread_mutex_lock(&_leave_mtx);

	if (++_leave_count < _num_instances) {
		pthread_cond_wait(&_leave, &_leave_mtx);
	} else {
		_leave_count = 0;
		if (!_passed) {
			post_verification(a);
		}
		if (break_point != 0) {
			break_point->deactivate(i, a);
			break_point = 0;
		}

		pthread_cond_broadcast(&_leave);
	}

	pthread_mutex_unlock(&_leave_mtx);

	if (_retry) {
		pthread_mutex_lock(&_help_mtx);

		if (++_help_count < _num_instances) {
			pthread_cond_wait(&_help, &_help_mtx);
		} else {
			_help_count = 0;
			pthread_cond_broadcast(&_help);
		}

		pthread_mutex_unlock(&_help_mtx);

		switch (_mode) {
			case Breakpointing:
				ret = breakpointing(i, t, a);
				break;
			case SingleStepping:
				ret = single_stepping(i, t, a);
				break;
			default:
				break;
		}

	} else {
		if (t->watchdog_suspended())
			t->watchdog_suspended(false);
	
		if (_passed) {
			if (t->its_me_who_passed_the_watchdog()) {
				/* this replica needs to re-enter in the original dmr handler */
				t->its_me_who_passed_the_watchdog(false);
				return Romain::RedundancyCallback::Watchdog_passed;
			}
		
			/* we resume the rest because they will encounter the same trap
			 * as the replica which passed the watchdog interrupt.
			 */
			if (t->watchdog_timeout() < Romain::Watchdog::SynchronizationTimeout) {
				reset(i, t, t->watchdog_timeout());
			} else {
				reset(i, t, Romain::Watchdog::SynchronizationTimeout);
			}

			L4::Cap<L4::Thread> vcpu_cap = t->vcpu_cap();
			vcpu_cap->vcpu_resume_commit(vcpu_cap->vcpu_resume_start());
		}
	}

	return ret;

}

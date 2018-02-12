#include <l4/sys/compiler.h>

#include "../configuration"
#include "../log"
#define MSG() DEBUGf(Romain::Log::Swifi)
#include "swifi.h"

Romain::SWIFIObserver *
Romain::SWIFIObserver::Create()
{ return new SWIFIPriv(); }


void Romain::SWIFIPriv::status() const { }

void
Romain::SWIFIPriv::startup_notify(Romain::App_instance *i,
                                  Romain::App_thread *,
                                  Romain::Thread_group *tg,
                                  Romain::App_model *a)
{
	/*
	 * We only inject faults in replica 0
	 */
	if (i->id() != 0)
		return;

	if (_breakpoint) {
		_breakpoint->activate(i, a);
		MSG() << "set SWIFI bp";
	}
}


Romain::Observer::ObserverReturnVal
Romain::SWIFIPriv::notify(Romain::App_instance *i,
                          Romain::App_thread *t,
                          Romain::Thread_group *tg,
                          Romain::App_model *a)
{
	Romain::Observer::ObserverReturnVal ret = Romain::Observer::Invalid;
	/*
	 * Faults are injected into replica 0 only. All other replicas
	 * simply get told to wait until the fault injector is done.
	 */
	if (i->id() != 0) {
		t->vcpu()->r()->ip--;
		return Romain::Observer::Finished_wait;
	}

	if (_breakpoint && _breakpoint->was_hit(t)) {
		INFO() << "\033[36m======== SWIFI: start (part 1) ========\033[0m";
		_breakpoint->deactivate(i,a);
		remove_breakpoint(t);
		/*
		 * Faults are injected into replica 0 only. All other replicas
		 * simply get told to wait until the fault injector is done.
		 */
		if (i->id() != 0) {
			return Romain::Observer::Finished_wait;
		}

		t->print_vcpu_state();

		switch(_flags) {
			case GPR:   _flipper = new GPRFlipEmulator(t->vcpu(), a, i);
						MSG() << "GPRFlipped";
						// GPR flipping does not use second phase
						ret = Romain::Observer::Finished_wakeup;
						break;
			case INSTR: _flipper = new InstrFlipEmulator(t->vcpu(), a, i);
						ret = Romain::Observer::Finished_step;
			            break;
			case ALU:   _flipper = new ALUFlipEmulator(t->vcpu(), a, i);
						ret = Romain::Observer::Finished_step;
			            break;
			case RAT:   _flipper = new RATFlipEmulator(t->vcpu(), a, i);
						ret = Romain::Observer::Finished_step;
						break;
			case MEM:   _flipper = new MemFlipEmulator(t->vcpu(), a, i);
						ret = Romain::Observer::Finished_step;
						break;
			default:
						ERROR() << "Unhandled injection type: " << _flags << "\n";
						return Romain::Observer::Ignored;
		}
		flipper_trap3(t);
		INFO() << "\033[36m======== SWIFI: end (part 1) ========\033[0m";

		return ret;

	} else if ((t->vcpu()->r()->trapno == 1) || 
			  ((t->vcpu()->r()->trapno == 3) && (a->in_trampoline(t->vcpu()->r()->ip))))
	{
		MSG() << "\033[36m======== SWIFI: start (part 2) ========\033[0m";
		switch(_flags) {
			case INSTR:
			case ALU:
			case RAT:
			case MEM:
				flipper_trap1(t);
				MSG() << "\033[36m======== SWIFI: end (part 2) ========\033[0m";
				return Romain::Observer::Finished_wakeup;
			case GPR: // not for us in this case
			case None:
				if (_flipper) delete _flipper;
				break;
		}
	}

	MSG() << "\033[36m======== SWIFI: end (part 2) ========\033[0m";
	return Romain::Observer::Ignored;
}


void Romain::SWIFIPriv::flipper_trap1(Romain::App_thread *t)
{
	MSG() << std::hex << t->vcpu()->r()->ip << " " << _flipper->prev_eip()
	      << " + " << _flipper->ilen();

	_flipper->revert();

	t->vcpu()->r()->ip = _flipper->next_eip();
	t->vcpu()->r()->flags &= ~TrapFlag;
	t->print_vcpu_state();

	delete _flipper;
}


void Romain::SWIFIPriv::flipper_trap3(Romain::App_thread *t, bool step)
{
	if (_flipper->flip()) {
		t->vcpu()->r()->flags |= TrapFlag;
	}
	t->print_vcpu_state();
}


Romain::SWIFIPriv::SWIFIPriv()
	: _breakpoint(0), _flags(None), _flipper(0)
{
	struct timeval tv;
	gettimeofday(&tv, 0);
	srandom(tv.tv_usec);

	l4_addr_t   target = ConfigIntValue("swifi:target");
	char const* inject = ConfigStringValue("swifi:inject");
	if (target && inject) {
		MSG() << "target: " << std::hex << target;
		MSG() << "   inj: " << inject;

		_breakpoint = new Breakpoint(target);
		if (strcmp(inject, "instr") == 0) {
			_flags = INSTR;
		} else if (strcmp(inject, "gpr") == 0) {
			_flags = GPR;
		} else if (strcmp(inject, "alu") == 0) {
			_flags = ALU;
		} else if (strcmp(inject, "rat") == 0) {
			_flags = RAT;
		} else if (strcmp(inject, "mem") == 0) {
			_flags = MEM;
		}
	}
}

/*
 * kiptime.cc --
 *
 *   Intercept and emulate accesses to Fiasco's KIP->clock field.
 *
 * (c) 2011-2013 Björn Döbel <doebel@os.inf.tu-dresden.de>,
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include "../log"
#include "../app_loading"
#include "../configuration"
//#include "../emulation"

#include "observers.h"

#include <vector>
#include <cstdlib>
#include <cstring>
#include <l4/libc_backends/clk.h>

namespace Romain {
class KipTimeObserver_priv : public KIPTimeObserver
{
	private:
		std::vector<Breakpoint*> _breakpoints;
		l4_umword_t              _hitcount;

		void configureBreakpoint(char *str)
		{
			errno = 0;
			l4_addr_t a = strtol(str, NULL, 16); // XXX check return
			if (errno) {
				ERROR() << "Conversion error: " << errno << std::hex << " (" << a << ")" << "\n";
			} else {
				_breakpoints.push_back(new Breakpoint(a));
			}			
		}

	DECLARE_OBSERVER("kip time");
	KipTimeObserver_priv();
};
}

Romain::KIPTimeObserver* Romain::KIPTimeObserver::Create()
{
	return new Romain::KipTimeObserver_priv();
}

Romain::KipTimeObserver_priv::KipTimeObserver_priv()
  : _hitcount(0)
{
	char *rtget = strdup(ConfigStringValue("kip-time:libc_backend_rt_clock_gettime", ""));
	if (strlen(rtget) != 0) {
		INFO() << "BP @ " << rtget;
		configureBreakpoint(rtget);
	} else {
		ERROR() << "No set address for libc_backend_rt_clock_gettime()\n";
		//enter_kdebug("??");
	}

	char *monoget = strdup(ConfigStringValue("kip-time:mono_clock_gettime", ""));
	if (strlen(monoget) != 0) {
		INFO() << "BP @ " << monoget;
		configureBreakpoint(monoget);
	} else {
		ERROR() << "No set address for mono_clock_gettime()\n";
		//enter_kdebug("??");
	}

	free(monoget);
	free(rtget);
	//enter_kdebug("kiptime");
}


void Romain::KipTimeObserver_priv::status() const
{
	INFO() << "[time] gettime() calls: " << _hitcount;
}

/*****************************************************************
 *                      Debugging stuff                          *
 *****************************************************************/
void Romain::KipTimeObserver_priv::startup_notify(Romain::App_instance *inst,
                                                  Romain::App_thread *,
                                                  Romain::Thread_group *,
                                                  Romain::App_model *am)
{
	for (auto i = _breakpoints.begin(); i != _breakpoints.end(); ++i) {
		DEBUG() << std::hex << (*i)->address();
		(*i)->activate(inst, am);
	}
}

Romain::Observer::ObserverReturnVal
Romain::KipTimeObserver_priv::notify(Romain::App_instance *i,
                                     Romain::App_thread *t,
                                     Romain::Thread_group *tg,
                                     Romain::App_model *am)
{
	if (!entry_reason_is_int3(t->vcpu(), i, am) &&
		!entry_reason_is_int1(t->vcpu()))
		return Romain::Observer::Ignored;

	Romain::Observer::ObserverReturnVal obsRet = Romain::Observer::Ignored;

#if BENCHMARKING
	unsigned long long t1, t2;
	t1 = l4_rdtsc();
#endif

	for (auto it = _breakpoints.begin(); it != _breakpoints.end(); ++it) {
		if ((*it)->was_hit(t)) {
			++_hitcount;
			DEBUG() << "{" << tg->name << "} " << "BP @ " << std::hex << (*it)->address() << " was hit.";
			DEBUG() << "{" << tg->name << "} " << "stack ptr 0x" << std::hex << t->vcpu()->r()->sp;

			l4_addr_t stack      = am->rm()->remote_to_local(t->vcpu()->r()->sp, i->id());
			l4_addr_t ret        = *(l4_addr_t*)stack;
			l4_addr_t specptr    = *(l4_addr_t*)(stack + 1 * sizeof(l4_addr_t));
			l4_addr_t spec_local = am->rm()->remote_to_local(specptr, i->id());
			DEBUG() << "{" << tg->name << "} " << "Retaddr " << std::hex << ret << ", ptr " << specptr;

			int funcReturnVal = libc_backend_rt_clock_gettime((struct timespec*)(spec_local));
			t->vcpu()->r()->ax   = funcReturnVal;

			/* We wrote data directly into the replica's address space. Now
			   we need to make sure the other replicas see the same value.
			 */
			for (l4_umword_t rep = 0; rep < Romain::_the_instance_manager->instance_count();
				 ++rep) {

				if (rep == i->id())
					continue;

				l4_addr_t specptr_rep = am->rm()->remote_to_local(specptr, rep);
				DEBUG() << "{" << tg->name << "} " << "memcpy " << std::hex << specptr_rep << " <- " << spec_local;
				memcpy((void*)specptr_rep, (void*)spec_local, sizeof(struct timespec));
			}

			t->return_to(ret);

			obsRet = Romain::Observer::Replicatable;
			break;
		}
	}

#if BENCHMARKING
	t2 = l4_rdtsc();
	t->count_kiptime(t2-t1);
#endif

	DEBUG() << "KIP ret " << obsRet;
	return obsRet;
}

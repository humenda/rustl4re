/*
 * trap_limit.cc --
 *
 * 	Observer counting traps and terminating the
 * 	application after a specified amount of calls.
 *
 * (c) 2012-2013 Björn Döbel <doebel@os.inf.tu-dresden.de>,
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include "../app_loading"
#include "observers.h"

namespace Romain
{
class TrapLimitObserver_priv : public TrapLimitObserver
{
	l4_mword_t _limit;
	l4_mword_t _callcount;

	bool limitIsValid() { return _limit != -1; }
	l4_mword_t  count()        { return _callcount; }
	l4_mword_t  limit()        { return _limit; }
	void increment()    { ++_callcount; }

	public:
		TrapLimitObserver_priv()
			: _callcount(0)
		{
			_limit = ConfigIntValue("general:max_traps");
			INFO() << "Limit: " << _limit;
		}

		DECLARE_OBSERVER("trap_limit");
};
}

DEFINE_EMPTY_STARTUP(TrapLimitObserver_priv)

void Romain::TrapLimitObserver_priv::status() const
{
	switch(_limit) {
		case -1:
			break;
		default:
			INFO() << "Trap Limit: " << _callcount
			       << " / " << _limit;
			break;
	}
}

Romain::Observer::ObserverReturnVal
Romain::TrapLimitObserver_priv::notify(Romain::App_instance *inst,
                                       Romain::App_thread *t,
                                       Romain::Thread_group *tg,
                                       Romain::App_model *a)
{
	if (limitIsValid()) {
		increment();
		//status();
		if (count() >= limit()) {
			enter_kdebug("*#^");
		}
	}

	// Replicatable, so that we only count this once
	return Romain::Observer::Continue;
}

Romain::TrapLimitObserver*
Romain::TrapLimitObserver::Create()
{
	return new TrapLimitObserver_priv();
}

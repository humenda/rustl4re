#include <l4/sys/compiler.h>

#include "../configuration"
#include "../log"
#include "observers.h"

#define MSG() DEBUGf(Romain::Log::MarkShared)

namespace Romain {
class MSO_priv : public Romain::MarkSharedObserver
{
	DECLARE_OBSERVER("mso");
	MSO_priv()
	{
		INFO() << "new MSO()";
	}

	static bool isMSOEntry(L4vcpu::Vcpu *vcpu)
	{
		return (vcpu->r()->trapno == 13) and ((vcpu->r()->err >> 3)== 0x42);
	}
};
}


Romain::MarkSharedObserver*
Romain::MarkSharedObserver::Create()
{
	return new MSO_priv();
}


DEFINE_EMPTY_STARTUP(MSO_priv);


void Romain::MSO_priv::status() const
{ }


Romain::Observer::ObserverReturnVal
Romain::MSO_priv::notify(Romain::App_instance *i,
                         Romain::App_thread *t,
                         Romain::Thread_group *tg,
                         Romain::App_model *a)
{
	Romain::Observer::ObserverReturnVal retval = Romain::Observer::Ignored;

	if (MSO_priv::isMSOEntry(t->vcpu())) {
		l4_addr_t addr = t->vcpu()->r()->ax;
		MSG() << "MSO: GPF found. Address " << std::hex << addr;
		auto n = a->rm()->find(addr);

		if (n) {
			Romain::Region_handler &rh = const_cast<Romain::Region_handler&>(n->second);
			MSG() << "Local: " << std::hex << rh.local_region(0).start()
			      << " - " << rh.local_region(0).end();

			// unmap everything
			for (l4_addr_t page = addr; page <= rh.local_region(0).end(); page += L4_SUPERPAGESIZE)
			{
				l4_fpage_t fp = l4_fpage(page, L4_SUPERPAGESHIFT, L4_FPAGE_RW);
				//INFO() << std::hex << page << " " << l4_debugger_global_id(i->vcpu_task().cap());
				chksys(i->vcpu_task()->unmap(fp, L4_FP_ALL_SPACES), "unmap");
			}

			MSG() << "Unmapped from replica.";

			// identity mapping? -> use c&e
			if (addr == rh.local_region(0).start()) {
				rh.writable(Romain::Region_handler::Copy_and_execute);
			} else {
				rh.writable(Romain::Region_handler::Read_only_emulate_write);
			}

			t->vcpu()->r()->ip += 2;

			retval = Romain::Observer::Finished;

		} else {
			ERROR() << "Application specified address " << addr << " in unmapped region.";
			abort();
		}
	}

	return retval;
}

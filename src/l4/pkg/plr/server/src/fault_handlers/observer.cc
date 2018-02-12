/*
 * observer.cc --
 *
 *    General fault observer functions
 *
 * (c) 2011-2013 Björn Döbel <doebel@os.inf.tu-dresden.de>,
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include "observers.h"
#include "../configuration"

Romain::Observer* 
Romain::ObserverFactory::CreateObserver(char const *name)
{
#define CASE(string, object) \
	do { \
		if (strcmp(name, string) == 0) { \
			return object; \
		} \
	} while (0)

	CASE("simple",     new SimpleDebugObserver());
	CASE("trap",       new TrapObserver());
	CASE("vcpu_state", new PrintVCPUStateObserver());
	CASE("pagefaults", new PageFaultObserver());
	CASE("syscalls",   new SyscallObserver());
	CASE("swifi",      SWIFIObserver::Create());
	CASE("kip-time",   KIPTimeObserver::Create());
	CASE("trap_limit", TrapLimitObserver::Create());
	CASE("threads",    PThreadLockObserver::Create());
	CASE("replicalog", new ReplicaLogObserver());
	CASE("mso",        MarkSharedObserver::Create());

#if 0
	if (strcmp(name, "gdb") == 0) {
		int port = ConfigIntValue("gdbstub:port");
		if (port > 0) { // want TCP connection
			return new Romain::GDBServerObserver(new Romain::TCPConnection(port));
		} else { // want serial line
			return new Romain::GDBServerObserver(new Romain::SerialConnection());
		}
	}
#endif

	enter_kdebug("no observer found.");
	return NULL;

#undef CASE
}


bool Romain::Observer::entry_reason_is_int3(L4vcpu::Vcpu* vcpu,
                                            Romain::App_instance *,
                                            Romain::App_model *am)
{
	/*
	 * Some other debugger component might already have
	 * replaced the opcode with sth. different (e.g., for emulating
	 * a breakpoint. In this case everything has become invalid
	 * right now and we skip, too.
	 */
	l4_addr_t remote = vcpu->r()->ip - 1;
	l4_uint8_t op = *(l4_uint8_t*)am->rm()->remote_to_local(remote, 0);
	return (vcpu->r()->trapno == 3) && (op == 0xCC);
}



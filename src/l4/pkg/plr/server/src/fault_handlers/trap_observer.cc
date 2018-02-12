/*
 * trap_observer.cc --
 *
 *     Implementation of the observer handling all traps, e.g.,
 *     system calls. Also, this observer handles Fiasco JDB
 *     traps.
 *
 * (c) 2011-2013 Björn Döbel <doebel@os.inf.tu-dresden.de>,
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include "../app_loading"
#include "observers.h"

char const * const Romain::TrapObserver::jdb_out_prefix = BOLD_GREEN;
char const * const Romain::TrapObserver::jdb_out_suffix = NOCOLOR;

DEFINE_EMPTY_STARTUP(TrapObserver)

void Romain::TrapObserver::status() const { }

Romain::Observer::ObserverReturnVal
Romain::TrapObserver::notify(Romain::App_instance *inst,
                             Romain::App_thread *t,
                             Romain::Thread_group *tg,
                             Romain::App_model *a)
{
	if (t->vcpu()->r()->trapno == 6) {
		ERROR() << "Invalid opcode @ " << std::hex << t->vcpu()->r()->ip << "\n";
		enter_kdebug("invalid opcode");
	}

	if (!entry_reason_is_int3(t->vcpu(), inst, a)) {
		return Romain::Observer::Ignored;
	}
#if BENCHMARKING
	unsigned long long t1, t2;
	t1 = l4_rdtsc();
#endif

	// address is already 1 _behind_ the INT3 opcode
	l4_addr_t local = a->rm()->remote_to_local(t->vcpu()->r()->ip, inst->id());
	unsigned op = *(unsigned char*)local;

	switch(op) {
		case 0xEB: // jmp -> enter_kdebug(text)
			{
				unsigned len = *(unsigned char*)(local+1);
				char strbuf[len+1];
				memcpy(strbuf, (unsigned char*)local+2, len);
				strbuf[len] = 0;
				INFO() << TrapObserver::jdb_out_prefix << "JDB:" << TrapObserver::jdb_out_suffix << " " << strbuf;
				enter_kdebug("enter_kdebug issued");

#if 0
				DEBUG() << std::hex << t->vcpu()->r()->ip;
				t->vcpu()->r()->ip += len;
				t->vcpu()->r()->ip += 2;
				DEBUG() << std::hex << t->vcpu()->r()->ip;
#endif
				//t->vcpu()->r()->ip -= 1;
			}
			break;
		case 0x3C: // outhex() & co.
			{
				unsigned char out_op = *(unsigned char*)(local+1);
				switch(out_op) {
					case 0: // outchar
						INFO() << TrapObserver::jdb_out_prefix << "JDB:" << TrapObserver::jdb_out_suffix << " " << (char)(t->vcpu()->r()->ax & 0xFF);
						break;
					case 2: // outstring
						{
							char buf[80];
							snprintf(buf, 80, "%s", (char*)a->rm()->remote_to_local(t->vcpu()->r()->ax, inst->id()));
							INFO() << TrapObserver::jdb_out_prefix << "JDB:" << TrapObserver::jdb_out_suffix << " " << buf;
							if (buf[strlen(buf)-1] == '\n') {
								enter_kdebug("jdb newline");
							}
						}
						break;
					case 5: // outhex32
						INFO() << TrapObserver::jdb_out_prefix << "JDB:" << TrapObserver::jdb_out_suffix << " " << std::hex << t->vcpu()->r()->ax;
						break;
					default: 
						ERROR() << "unhandled out op: " << std::hex << (int)out_op << "\n";
						break;
				};
			}
			break;
		default:
#if 0
			ERROR() << "Unhandled JDB opcode: 0x" << std::hex << op << "\n";
			break;
#else
#if BENCHMARKING
	t2 = l4_rdtsc();
	t->count_traps(t2-t1);
#endif
			return Romain::Observer::Ignored;
#endif
	}

	//enter_kdebug("int 3");
	return Romain::Observer::Replicatable;
}

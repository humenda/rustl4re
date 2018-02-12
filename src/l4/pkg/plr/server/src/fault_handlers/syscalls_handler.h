#pragma once

/*
 * syscalls_handler.h --
 *
 *     Interface for system call wrappers
 *
 * (c) 2012-2013 Björn Döbel <doebel@os.inf.tu-dresden.de>,
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

 #include "../manager"

namespace Romain
{

/*
 * Generic system call wrapper class
 */
class SyscallHandler
{

	/*
	 * Store UTCB from orig to backup.
	 */
	void store_utcb(char *orig, char *backup) { memcpy(backup, orig, L4_UTCB_OFFSET); }

	public:
		SyscallHandler() = default;

		virtual
		Romain::Observer::ObserverReturnVal
		handle(Romain::App_instance *i,
		       Romain::App_thread *t,
		       Romain::Thread_group *tg,
		       Romain::App_model *a)
		{
			enter_kdebug("calling empty syscall handler!?");
			return Romain::Observer::Replicatable;
		}

		virtual void
		proxy_syscall(Romain::App_instance *i,
		              Romain::App_thread *t,
		              Romain::Thread_group *tg,
		              Romain::App_model *a);
};

#define SpecificSyscallHandler(name) \
	class name : public SyscallHandler \
	{ \
		public: \
			name() \
				: SyscallHandler() \
			{ } \
			virtual \
			Romain::Observer::ObserverReturnVal \
			handle(Romain::App_instance *i, \
			       Romain::App_thread *t, \
			       Romain::Thread_group *tg, \
			       Romain::App_model *a); \
	};


/*
 * System call wrapper for L4_PROTO_SCHEDULER
 */
SpecificSyscallHandler(Scheduling);
SpecificSyscallHandler(ThreadHandler);
SpecificSyscallHandler(RegionManagingHandler);
SpecificSyscallHandler(IrqHandler);
SpecificSyscallHandler(IrqSenderHandler);

}

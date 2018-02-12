#pragma once

/*
 * observer.h --
 *
 *    Fault observer interface
 *
 * (c) 2011-2013 Björn Döbel <doebel@os.inf.tu-dresden.de>,
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */


#include "../fault_observers"
#include "debugging.h" // Breakpoint
#if 0
#include "../gdb_stub/gdbserver"
#include "../gdb_stub/connection"
#endif
#include "../configuration"

namespace Romain
{
	/*
	 * Debug observer - can set breakpoint on a single instruction and
	 * single-steps the application after hitting the BP.
	 */
	class SimpleDebugObserver : public Observer
	{
		private:
			Breakpoint *_bp;
			l4_umword_t _int1_seen;

			l4_addr_t determine_address()
			{
				l4_addr_t addr = ConfigIntValue("simpledbg:singlestep");
				DEBUG() << "Single step target: 0x"
				        << std::hex << addr;

				return addr;
			}

			DECLARE_OBSERVER("simple dbg");
			SimpleDebugObserver();
	};


	/*
	 * INT3 trap observer (aka JDB emulator)
	 */
	class TrapObserver : public Observer
	{
		DECLARE_OBSERVER("trap");
		static char const * const jdb_out_prefix;
		static char const * const jdb_out_suffix;
	};


	/*
	 * Observer that prints VCPU content on every fault.
	 */
	class PrintVCPUStateObserver : public Observer
	{
		DECLARE_OBSERVER("print vcpu state");
	};


	/*
	 * Observer for handling page faults.
	 */
	class PageFaultObserver : public Observer
	{
		private:
		bool _readonly;

		l4_umword_t fit_alignment(Romain::Region const * local,
		                          L4Re::Util::Region const * remote,
		                          l4_umword_t offset, Romain::App_thread *t);

		DECLARE_OBSERVER("pf");
		PageFaultObserver();

		bool always_readonly() const { return _readonly; }
	};


	/*
	 * System call handling
	 */
	class SyscallObserver : public Observer
	{
		DECLARE_OBSERVER("syscalls");

		SyscallObserver()
			: _replica_thread_groups()
		{ }

		protected:

		std::map<l4_umword_t, Romain::Thread_group*> _replica_thread_groups;

		/*******************************************
		 * Thread-related system calls
		 *******************************************/
		Romain::Observer::ObserverReturnVal
			handle_task(Romain::App_instance*, Romain::App_thread*, Romain::App_model*);
	};


	/*
	 * Limit for the amount of traps handled before exiting
	 *
	 * Debugging feature: for unit tests we want to run an application for a
	 * dedicated amount of system calls until terminating the run, so that the
	 * output becomes reproducible.
	 */
	class TrapLimitObserver : public Observer
	{
		public:
		static TrapLimitObserver *Create();
	};


	/*
	 * Observer monitoring accesses to the clock field in the KIP.
	 *
	 * The KIP is mostly read-only and we may therefore get along with a single copy
	 * that is mapped read-only to all clients. However, the KIP contains a clock field
	 * that is updated on every context switch if the kernel has been compiled with
	 * FINE_GRAIN_CPU_TIME. This field is used by gettimeofday(). In order to let all
	 * replicas see the same time when they use this function, we need to instrument
	 * accesses to the clock field.
	 */
	class KIPTimeObserver : public Observer
	{
		public:
		static KIPTimeObserver *Create();
	};


	/*
	 * DEBUG: make memory inaccessible on demand (e.g., mark as shared region)
	 */
	class MarkSharedObserver: public Observer
	{
		public:
		static MarkSharedObserver* Create();
	};


	/*
	 * Software-Implemented Fault Injection
	 */
	class SWIFIObserver : public Observer
	{
		public:
		static SWIFIObserver* Create();
	};


	class PThreadLockObserver : public Observer
	{
		public:
		static PThreadLockObserver* Create();
	};


	class ReplicaLogObserver : public Observer
	{
		DECLARE_OBSERVER("replica::log");

		public:
			ReplicaLogObserver();
			int timeout() { return _timeout; }

			bool want_cancel() { return _cancel; }

		private:
			struct {
				l4_addr_t local_addr;
			} buffers[Romain::MAX_REPLICAS];
			l4_mword_t _timeout;
			bool _cancel;
			pthread_t _to_thread;

			void map_eventlog(Romain::App_instance *i, l4_mword_t logsizeMB);
			void dump_eventlog(l4_umword_t id) const;
	};
}

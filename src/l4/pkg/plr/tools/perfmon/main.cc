#include <l4/plr/perf.h>
#include <l4/plr/cpuid.h>
#include <l4/sys/debugger.h>
#include <l4/sys/scheduler>
#include <l4/sys/thread>
#include <l4/re/error_helper>
#include <l4/re/env>
#include <l4/util/util.h>
#include <l4/util/rdtsc.h>
#include <pthread-l4.h>

#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <vector>
#include <algorithm>

enum {
	MAX_CPUS = 32
};

using L4Re::chksys;

/*
 * Map logical (=sequential) CPU numbers to the topology of the underlying
 * platform.
 *
 * Background: Fiasco.OC uses sequential CPU numbers to assign threads to CPUs. These
 *             numbers are assigned during bootup and their order depends on the order
 *             in which booted CPUs come up. This means, assigning two threads to CPUs
 *             0 and 1 can in one case mean that they are running on the same core in
 *             different hyperthreads, while in the next run, they run on the different
 *             physical CPUs (or even NUMA domains, if we had those).
 *
 *             The purpose of this map is to make the assignment less surprising, e.g.,
 *             for the same platform, assigning to two specific CPU IDs should always
 *             lead to the same performance.
 */
class LogicalCPUMap
{
	protected:
	std::vector<CPU_id> _fiasco_cpus;
	unsigned            _cpus;

	static void* topology_scanner(void* data)
	{
		l4_debugger_set_object_name(pthread_l4_cap(pthread_self()), "romain::topscan");
		LogicalCPUMap *m = reinterpret_cast<LogicalCPUMap*>(data);
		L4::Cap<L4::Thread> thread = Pthread::L4::cap(pthread_self());

		for (unsigned cpu = 0; cpu < m->num_cpus(); ++cpu) {
			l4_sched_param_t sp = l4_sched_param(2);
			sp.affinity = l4_sched_cpu_set(cpu, 0);
			chksys(L4Re::Env::env()->scheduler()->run_thread(thread, sp));

			m->_fiasco_cpus[cpu] = CPUID::current_apicid(cpu);
		}
		return 0;
	}


	void scan_topology()
	{
		/* Check if we can perform a topology query. */
		if (CPUID::max_cpuid() < CPUID::TOPOLOGY) {
			std::cout << "CPU does not provide topology information\n";
			return;
		}

		pthread_t p;
		int ret = pthread_create(&p, 0, LogicalCPUMap::topology_scanner, this);
		if (ret != 0) {
			std::cout << "error creating topology scanner\n";
			exit(1);
		}
		ret = pthread_join(p, 0);

		for (unsigned i = CPUID::num_cpus(); i < MAX_CPUS; ++i) {
			_fiasco_cpus[i].apic_id = 0xDEADBEEF;
		}

		std::cout << "Topology scan completed.\n";
	}


	void print_map()
	{
		std::cout << "Virtual to Fiasco CPU map\n";

		for (unsigned i = 0; i < CPUID::num_cpus(); ++i) {
			std::cout << "virt# " << i << " fiasco# " << _fiasco_cpus[i].fiasco_id
				      << " apic# " << std::hex << _fiasco_cpus[i].apic_id << "\n";
		}
	}

	public:
		LogicalCPUMap(unsigned max = MAX_CPUS)
			: _fiasco_cpus(max), _cpus(max)
		{
			/* Default: identity mapping if nothing else works */
			for (unsigned i = 0; i < MAX_CPUS; ++i) {
				_fiasco_cpus[i].fiasco_id = i;
				_fiasco_cpus[i].apic_id   = i;
			}

			if (CPUID::have_cpuid()) {
				_cpus = CPUID::num_cpus();
				scan_topology();
				std::sort(_fiasco_cpus.begin(), _fiasco_cpus.end(), CPU_id::comp_apic);
				print_map();
			}

			//enter_kdebug("cpu");

		}

		unsigned logicalToCPU(unsigned log) const
		{
			std::cout << log << " -> " << _fiasco_cpus[log].fiasco_id << "\n";
			return _fiasco_cpus[log].fiasco_id;
		}

		unsigned num_cpus() const { return _cpus; }
};


class CounterLog
{
	struct perf_entry {
		l4_uint64_t timestamp;
		l4_uint64_t v1, v2, v3, v4;
	};
	
	LogicalCPUMap const *map;

	unsigned             entr;
	perf_entry          *thelog;
	unsigned             logIndex;

	public:

		LogicalCPUMap const * cpu_map() { return map; }
		unsigned entries() { return entr; }

		void loginit()
		{
			L4::Cap<L4::Thread> thread = Pthread::L4::cap(pthread_self());

			/* #1: Setup counters. */
			for (unsigned cpu = 0; cpu < map->num_cpus(); ++cpu) {
				l4_sched_param_t sp = l4_sched_param(2);
				sp.affinity = l4_sched_cpu_set(cpu, 0);
				chksys(L4Re::Env::env()->scheduler()->run_thread(thread, sp));
				perfconf();
			}
		}

		void *logwork()
		{
			L4::Cap<L4::Thread> thread = Pthread::L4::cap(pthread_self());
			/* #2 Counter read loop */
			for (unsigned cpu = 0; cpu < map->num_cpus(); ++cpu) {
				l4_sched_param_t sp = l4_sched_param(4);
				sp.affinity = l4_sched_cpu_set(cpu, 0);
				chksys(L4Re::Env::env()->scheduler()->run_thread(thread, sp));

				perf_entry *ent = &thelog[logIndex * map->num_cpus() + cpu];
				ent->timestamp = l4_rdtsc();
				perfread2(&ent->v1, &ent->v2, &ent->v3, &ent->v4);
			}

			++logIndex;

			return 0;
		}

		CounterLog(LogicalCPUMap const *m, unsigned entries)
			: map(m), entr(entries), logIndex(0)
		{
			thelog = new perf_entry[m->num_cpus() * entries];
			l4_touch_rw(thelog, m->num_cpus() * entries * sizeof(perf_entry));

			loginit();
		}


		void dump()
		{
			for (unsigned i = 0; i < map->num_cpus(); ++i) {
				std::cout << "\033[33;1m======== CPU " << i << " =====\033[0m\n";
				l4_uint64_t tsc_base = thelog[i].timestamp;
				for (unsigned ent = 0; ent < entr; ++ent) {
					perf_entry * e = &thelog[ent * map->num_cpus() + i];
					printf("%016lld %16lld %16lld %16lld %16lld\n",
					       e->timestamp - tsc_base, e->v1, e->v2, e->v3, e->v4);
				}
			}
		}
};


int main(int argc, char **argv)
{
	unsigned sleepInterval = 2000;
	unsigned numEntries    = 200;
	LogicalCPUMap *map;
	CounterLog    *log;

	std::cout << "===== Perf Mon ====\n";
	l4_sleep(2000); // wait for bootup to complete (XXX)

	if (argc > 1) {
		numEntries = strtol(argv[1], 0, 10);
	}

	if (argc > 2) {
		sleepInterval = strtol(argv[2], 0, 10) * 1000;
	}

	map = new LogicalCPUMap();
	log = new CounterLog(map, numEntries);
	std::cout << std::dec << "   Loops " << numEntries << ", interval "
	          << sleepInterval << " ms. CPUs: " << map->num_cpus() << "\n";

	for (unsigned i = 0; i < numEntries; ++i) {
		log->logwork();
		l4_sleep(sleepInterval);
	}

	std::cout << "\033[33;1mPerfmon done. Dump.\033[0m\n";

	log->dump();

	return 0;
}

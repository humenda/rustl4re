#include "log"
#include <l4/plr/measurements.h>
#include <pthread-l4.h>
#include <l4/util/rdtsc.h>
#include <l4/sys/debugger.h>
#include <l4/sys/kdebug.h>
#include <l4/sys/scheduler>
#include <l4/re/env>
#include <l4/re/error_helper>

using L4Re::chksys;

struct timerArgs
{
	l4_umword_t cpu;
	l4_addr_t timer;
};


static timerArgs global_arg;

void *timerThread(void *argp)
{
	char const *name = "Romain::timer";
	L4::Cap<L4::Thread> self = Pthread::L4::cap(pthread_self());
	l4_debugger_set_object_name(self.cap(), name);

	l4_sched_param_t sp = l4_sched_param(2);
	sp.affinity = l4_sched_cpu_set(global_arg.cpu, 0);
	chksys(L4Re::Env::env()->scheduler()->run_thread(self, sp));

	INFO() << "Timer thread. CPU " << global_arg.cpu << ". Timestamp @ 0x" << std::hex << global_arg.timer;

	while (1) {
		*((volatile l4_uint64_t*)global_arg.timer) = l4_rdtsc();
	}

	return 0;
}

void
Measurements::EventBuf::launchTimerThread(l4_addr_t timer, l4_umword_t CPU)
{
	pthread_t tmr;

	global_arg.timer = timer;
	global_arg.cpu   = CPU;

	l4_mword_t err = pthread_create(&tmr, 0, timerThread, (void*)&global_arg);
	if (err) {
		ERROR() << "Error creating timer thread: " << err << "\n";
	}
}

/*
 * replicalog.cc --
 *
 *     Fault observer that attaches a magic event buffer
 *     (see measurements.h) to each replica and dumps the replicas'
 *     event buffers after the program terminates.
 *
 * (c) 2013 Björn Döbel <doebel@os.inf.tu-dresden.de>,
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include "observers.h"
#include "../configuration"
#include "../memory"
#include "../manager"
#include "../log"

#include <l4/plr/uu.h>
#include <l4/plr/measurements.h>

namespace Romain {
	extern InstanceManager *_the_instance_manager;
}


void *to_thread(void *arg)
{
	Romain::ReplicaLogObserver *o = reinterpret_cast<Romain::ReplicaLogObserver *>(arg);
	INFO() << "Waiting for " << o->timeout() << " seconds.";
	sleep(o->timeout());

	if (o->want_cancel())
		return NULL;

	l4_debugger_set_object_name(pthread_l4_cap(pthread_self()), "romain::timout");

#if EVENT_LOGGING
	Measurements::GenericEvent* ev = Romain::globalLogBuf->next();
	ev->header.tsc                 = Romain::globalLogBuf->getTime(Romain::Log::logLocalTSC);
	ev->header.vcpu                = (l4_uint32_t)0xDEADBEEF;
	ev->header.type                = Measurements::Thread_stop;
#endif

	Romain::_the_instance_manager->show_stats();

	INFO() << "abort";

	enter_kdebug("abort after logdump");

	return NULL;
}


Romain::ReplicaLogObserver::ReplicaLogObserver()
	: _cancel(false)
{
	_timeout = ConfigIntValue("general:logtimeout");
	if (_timeout > 0) {
		int err = pthread_create(&_to_thread, NULL, to_thread, this);
		_check(err != 0, "error creating timeout thread");
	}
	for (l4_umword_t i = 0; i < Romain::MAX_REPLICAS; ++i) {
		buffers[i].local_addr = 0;
	}
}


void
Romain::ReplicaLogObserver::map_eventlog(Romain::App_instance *i, l4_mword_t logsizeMB)
{
	l4_mword_t size = logsizeMB << 20;
	l4_umword_t mapops = 0;
	l4_addr_t local_map_addr = buffers[i->id()].local_addr;
	l4_addr_t remote_map_addr = Romain::REPLICA_LOG_ADDRESS;

	/* 1. map the TSC shared page read-only */
	INFO() << "shared tsc @ " << Romain::globalLogBuf->timestamp;
	i->map_aligned(reinterpret_cast<l4_addr_t>(Romain::globalLogBuf->timestamp),
	               Romain::REPLICA_TSC_ADDRESS, L4_PAGESHIFT, L4_FPAGE_RO);

	/* Now initialize the shared event buffer */
	Measurements::EventBuf* buf = reinterpret_cast<Measurements::EventBuf*>(buffers[i->id()].local_addr);
	buf->index = 0;
	buf->sharedTSC = true;
	buf->set_buffer((l4_uint8_t*)remote_map_addr + sizeof(Measurements::GenericEvent), (logsizeMB << 20) - sizeof(Measurements::GenericEvent));
	INFO() << buf->index << " " << buf->size << std::endl;

	while (size > 0) {

		l4_umword_t sz, shift;

		if (size >= (4 << 20)) { // map 4 MB page
			sz = L4_SUPERPAGESIZE;
			shift = L4_SUPERPAGESHIFT;
		} else { // map 4 kB page
			sz = L4_PAGESIZE;
			shift = L4_PAGESHIFT;
		}

		i->map_aligned(local_map_addr, remote_map_addr, shift, L4_FPAGE_RW);
		size            -= sz;
		local_map_addr  += sz;
		remote_map_addr += sz;
		mapops          += 1;
	}

	INFO() << "Mapped to replica. Used " << mapops << " map() operations";
}


void Romain::ReplicaLogObserver::startup_notify(Romain::App_instance *i,
					                            Romain::App_thread *t,
					                            Romain::Thread_group *tg,
					                            Romain::App_model *a)
{
	static bool logregion_reserved = false;

	l4_mword_t logMB = ConfigIntValue("general:replicalogsize");
	if (logMB == -1) { // use general logbuf size if no specific size was set
		logMB = ConfigIntValue("general:logbuf");
	}

	INFO() << "Replica log size: " << logMB << " MB";

	/* Reserve the area at the replicas' RM. We map this buffer without going through
	   the official channels. */
	if (!logregion_reserved) {
		a->rm()->attach_area(Romain::REPLICA_LOG_ADDRESS, logMB << 20, 0, L4_SUPERPAGESHIFT);
		logregion_reserved = true;
	}

	L4::Cap<L4Re::Dataspace> mem;
	buffers[i->id()].local_addr = Romain::Region_map::allocate_and_attach(&mem, logMB << 20, 0, L4_SUPERPAGESHIFT);
	INFO() << "Buffer for instance " << i->id() << " @ " << std::hex << buffers[i->id()].local_addr;
	l4_touch_rw(reinterpret_cast<void*>(buffers[i->id()].local_addr), logMB << 20);

	map_eventlog(i, logMB);

	//enter_kdebug("LogObserver::startup");
}


Romain::Observer::ObserverReturnVal
Romain::ReplicaLogObserver::notify(Romain::App_instance *i,
                                   Romain::App_thread *t,
                                   Romain::Thread_group *tg,
                                   Romain::App_model *a)
{
	/* We don't handle any events. */
	return Romain::Observer::Ignored;
}


void
Romain::ReplicaLogObserver::dump_eventlog(l4_umword_t id) const
{
	// prevent a potential timeout thread from dumping in parallel
	const_cast<Romain::ReplicaLogObserver*>(this)->_cancel = true;

	INFO() << "Dumping ... " << id;

	Measurements::EventBuf *buf = reinterpret_cast<Measurements::EventBuf*>(buffers[id].local_addr);
	INFO() << "  event buffer @ " << std::hex << buf;

	char filename[32];
	snprintf(filename, 32, "replica%d.log", id);

	INFO() << "file: " << filename;

	l4_umword_t oldest = buf->oldest();

	INFO() << "oldest: " << oldest;

	l4_umword_t dump_start, dump_size;
	if (!oldest) {
		dump_start = 0;
		dump_size  = buf->index * sizeof(Measurements::GenericEvent);
	} else {
		dump_start = oldest * sizeof(Measurements::GenericEvent);
		dump_size  = buf->size * sizeof(Measurements::GenericEvent);
	}

	/* buf addr is relocated in replica AS -> need to retransform */
	char *bufaddr = ((l4_addr_t)buf->buffer - Romain::REPLICA_LOG_ADDRESS) + (char*)buf;

	INFO() << "file: " << filename << " start " << std::hex << dump_start << " size " << dump_size;
	uu_dumpz_ringbuffer(filename, bufaddr,
	                    buf->size * sizeof(Measurements::GenericEvent),
	                    dump_start, dump_size);
}


void
Romain::ReplicaLogObserver::status() const
{
	for (l4_umword_t i = 0; i < Romain::MAX_REPLICAS; ++i) {
		if (buffers[i].local_addr != 0) {
			dump_eventlog(i);
		}
	}
	//enter_kdebug("LogObserver::status");
}

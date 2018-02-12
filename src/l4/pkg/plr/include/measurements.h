#pragma once

/*
 * measurements.h --
 *
 *     Event logging infrastructure
 *
 * (c) 2012-2013 Björn Döbel <doebel@os.inf.tu-dresden.de>,
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <cassert>
#include <climits>
//#include <utility>

#include <l4/util/atomic.h>
#include <l4/util/rdtsc.h>
#include <l4/sys/kdebug.h>

/*
 * Namespace containing classes regarding measurements and
 * sensors etc.
 *
 * XXX: C compatibility! Don't use C++ features except inside __cplusplus parts -> I want to
 *      use this header file from C files, too
 */
#ifdef __cplusplus
namespace Measurements
{
#endif

	enum EventTypes {
		Invalid = 0,
		Syscall = 1,
		Pagefault = 2,
		Swifi = 3,
		Foo = 4,
		Trap = 5,
		Thread_start = 6,
		Thread_stop  = 7,
		Locking = 8,
		SHMLocking = 9,
		Map = 10,
	};

	struct __attribute__((packed))
	SensorHead {
		l4_uint64_t   tsc;   // TSC: stop
		l4_uint32_t   vcpu;  // vcpu ptr
		l4_uint8_t    type;  // event type
	};


	struct __attribute__((packed))
	PagefaultEvent
	{
		char rw;
		l4_addr_t address;
		l4_addr_t localbase;
		l4_addr_t remotebase;
	};


	struct __attribute__((packed))
	MapEvent
	{
		l4_addr_t local;
		l4_addr_t remote;
		l4_size_t size;
		char unmap;
	};


	struct __attribute__((packed))
	TrapEvent
	{
		char        start;
		l4_addr_t   trapaddr;
		l4_uint32_t trapno;
	};


	struct __attribute__((packed))
	ThreadStartEvent
	{
		l4_addr_t   startEIP;
	};


	struct __attribute__((packed))
	SyscallEvent
	{
		l4_addr_t   eip;
		l4_uint32_t label;
	};


	struct __attribute__((packed))
	FooEvent
	{
		l4_umword_t start;
	};


	struct __attribute__((packed))
	LockEvent
	{
#ifdef __cplusplus
		enum LockEvents {
			lock,
			unlock,
			mtx_lock,
			mtx_unlock
		};
#endif
		l4_umword_t eventType;
		l4_umword_t lockPtr;
	};


	struct __attribute__((packed))
	SHMLockEvent
	{
		l4_umword_t lockid;
		l4_umword_t epoch;
		l4_umword_t owner; // current owner
		l4_umword_t type; // 1 -> init
			           // 2 -> lock_enter
					   // 3 -> lock_exit
			           // 4 -> unlock_enter
					   // 5 -> unlock_exit
	};


	struct __attribute__((packed))
	BarnesEvent
	{
		l4_umword_t ptr;
		l4_umword_t num;
		l4_umword_t type;
	};


	struct __attribute__((packed))
	GenericEvent
	{
		struct SensorHead header;
		union {
			struct PagefaultEvent pf;
			struct MapEvent       map;
			struct SyscallEvent sys;
			struct FooEvent     foo;
			struct TrapEvent    trap;
			struct ThreadStartEvent threadstart;
			struct LockEvent    lock;
			struct SHMLockEvent shml;
			struct BarnesEvent  barnes;
			char         pad[19];
		} data;
	};


	/*
	 * Event buffer
	 * 
	 * An event buffer holds events of the GenericEvent type. The class does not
	 * allocate memory. Instead the underlying buffer needs to be specified using
	 * the set_buffer() function.
	 * 
	 * Once the buffer is valid, users obtain an element using the next() method
	 * and fill it appropriately.
	 * 
	 * The buffer is managed as a ring buffer and may overflow, in which case the
	 * oldest elements get overwritten. The index variable is increased monotonically,
	 * so users may determine whether the buffer has already overflown by checking
	 * if index > size. If so, (index mod size) points to the oldest element.
	 *
	 * The whole buffer can be dumped using the dump() method. This will produce a UU-encoded
	 * version of the zipped buffer content.
	 */
	struct __attribute__((packed))
	EventBuf
	{
		struct GenericEvent*  buffer;
		l4_uint32_t       index;
		l4_umword_t       size;
		l4_umword_t       sharedTSC;
		l4_uint64_t   *timestamp;
		char _end[];

#ifdef __cplusplus

		/**
		 * Create event buffer
		 *
		 * sharableTSC -> allow the TSC value to be located in a way that we can share
		 *                this value among different address spaces (e.g., have replicas
		 *                log events themselves using this TSC). This requires the timestamp
		 *                value to be placed on a dedicated page.
		 */
		EventBuf(bool sharableTSC = false)
			: buffer(0), index(0), size(0), sharedTSC(sharableTSC ? 1 : 0)
		{
		  static_assert(sizeof(SensorHead) == 13, "Sensor head not 13 bytes large!");
		  static_assert(sizeof(GenericEvent) == 32, "GenericEvent larger than 24 bytes!");
		  //static_assert((l4_umword_t)((EventBuf const *)0)->_end < sizeof(GenericEvent), "head too large?");

		  if (!sharableTSC) {
		  	timestamp = new l4_uint64_t();
		  }

		  static l4_uint8_t      dummyBuffer[32];
		  set_buffer(dummyBuffer, 32);
		}


		~EventBuf()
		{
			enter_kdebug("~EventBuf");
			if (!sharedTSC) {
				delete timestamp;
			}
		}


		void set_buffer(l4_uint8_t    *buf, l4_umword_t size_in_bytes)
		{
			buffer = reinterpret_cast<GenericEvent*>(buf);
			size   = size_in_bytes / sizeof(GenericEvent);
		}


		void set_tsc_buffer(l4_uint64_t *buf)
		{
			timestamp = buf;
		}


		static void launchTimerThread(l4_addr_t timerAddress, l4_umword_t CPU);

		l4_uint64_t getTime(bool local=false)
		{
			if (local) {
				return l4_rdtsc();
			} else {
				return *timestamp;
			}
		}

		/*
		 * Get the next buffer entry.
		 *
		 * Safe against concurrent calls by using atomic increment on the
		 * counter.  Concurrent accesses may lead to events not being properly
		 * ordered, though.
		 */
		GenericEvent* next()
		{
			l4_umword_t val = l4util_inc32_res(&index) - 1;
			val %= size;
			return &buffer[val];
		}


		l4_umword_t oldest() const
		{
			if (index < size) {
				return 0;
			}
			else {
				return index % size;
			}
		}
#endif /* C++ */
	};

#ifdef __cplusplus
}

extern "C"
{
#endif
	void *evbuf_get_address(void);
	l4_uint64_t evbuf_get_time(void *eb, l4_umword_t is_local);
	struct GenericEvent* evbuf_next(void *eb);
#ifdef __cplusplus
}
#endif

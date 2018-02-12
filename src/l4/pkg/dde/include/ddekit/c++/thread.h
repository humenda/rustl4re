/*
 * \brief   DDEKit Threads
 * \author  Thomas Friebel <tf13@os.inf.tu-dresden.de>
 * \author  Christian Helmuth <ch12@os.inf.tu-dresden.de>
 * \author  Bjoern Doebel<doebel@os.inf.tu-dresden.de>
 * \date    2008-08-26
 */

/*
 * (c) 2006-2008 Technische Universität Dresden
 * This file is part of TUD:OS, which is distributed under the terms of the
 * GNU General Public License 2. Please see the COPYING file for details.
 */

#pragma once

/** \defgroup DDEKit_threads */

#include <l4/sys/compiler.h>
#include <l4/sys/types.h>

#include <l4/dde/ddekit/lock.h>
#include <l4/dde/ddekit/memory.h>
#include <l4/dde/ddekit/printf.h>
#include <l4/dde/ddekit/c++/memory.h>
#include <l4/dde/ddekit/c++/lock.h>

namespace DDEKit
{
	class Thread
	{
		public:
			Thread(void (*func)(void*), void *arg, char const * const name) { }
			virtual ~Thread() { }

			static void init_threading();

			// thread-global functions
			static Thread* create(char const *name, void (*func)(void*), void *arg);
			static Thread* myself();
			static void msleep(unsigned long msec);
			static void usleep(unsigned long usec);
			static void nsleep(unsigned long nsec);
			static void yield();
			static void schedule();
			static void __attribute__((noreturn)) exit();

			static int tls_key_thread;
			static DDEKit::Slab *stack_slab;

			virtual void run() = 0;
			virtual void setup_self() = 0;
			virtual l4_addr_t get_data() const = 0;
			virtual void set_data(l4_addr_t) = 0;
			virtual char const * const name() const = 0;
			virtual int  id() const = 0;
			virtual void terminate() = 0;

			static void sleep_on_lock(DDEKit::Lock const & lock);
			virtual void wakeup() const = 0;

		protected:
			Thread(const Thread&) { }
			Thread& operator=(const Thread& t) { return *this; }
	};
}

#include <l4/dde/ddekit/thread.h>
#include <l4/dde/ddekit/condvar.h>
#include <l4/dde/ddekit/panic.h>
#include <l4/dde/ddekit/memory.h>
#include <l4/dde/ddekit/printf.h>

#include <l4/dde/dde.h>
#include <l4/thread/thread.h>
#include <l4/log/log.h>
#include <l4/util/rdtsc.h>
#include <l4/util/util.h>
#include <l4/sys/thread.h>
#include <l4/sys/kdebug.h>

#include <cstdlib>
#include <cstring>
#include <cstdio>

#include "internal.h"

namespace DDEKit
{
	class Thread_impl : public Thread, public DDEKitObject
	{
		public:
			Thread_impl(char const * const name, void (*func)(void*), void *arg);
			virtual ~Thread_impl();
			
			static void exit() __attribute__((noreturn));
			static Thread_impl* myself();

			virtual void run();
			virtual void setup_self();
			virtual l4_addr_t get_data() const { return _data; }
			virtual void set_data(l4_addr_t d) { _data = d; }
			virtual char const * const name() const { return _name; }
			virtual int  id() const;
			virtual void terminate();

			void do_sleep_on_lock(DDEKit::Lock const & lock) const;
			virtual void wakeup() const;

			void execute() { _func(_arg); }

			static int const STACK_SIZE = 0x2000;

		protected:
			Thread_impl(const Thread_impl&) : Thread(0, 0, "") { }
			Thread_impl& operator=(const Thread_impl& t) { return *this; }

			l4thread_t   _l4thread;
			l4_addr_t    _data;
			l4_addr_t    _stack;
			Cond_var *   _sleep_cv;
			char *       _name;
			void        (*_func)(void*);
			void *       _arg;

			static void thread_startup(void *arg);
	};

	int Thread::tls_key_thread = -1;
	DDEKit::Slab* Thread::stack_slab = 0;
}

DDEKit::Thread* DDEKit::Thread::create(char const *name, void (*func)(void*), void *arg)
{
	return new DDEKit::Thread_impl(name, func, arg);
}


void DDEKit::Thread::exit()
{
	DDEKit::Thread_impl::exit();
}


void DDEKit::Thread_impl::exit()
{
	l4thread_exit();
}


void DDEKit::Thread::msleep(unsigned long msecs)
{
	l4thread_sleep(msecs);
}


void DDEKit::Thread::usleep(unsigned long usecs)
{
	l4_busy_wait_us(usecs);
}


void DDEKit::Thread::nsleep(unsigned long nsecs)
{
	l4_busy_wait_ns(nsecs);
}


void DDEKit::Thread::schedule(void)
{
	l4_thread_yield();
}


void DDEKit::Thread::yield(void)
{
	l4_thread_yield();
}


void DDEKit::Thread_impl::run()
{
	char l4name[20];
	l4thread_t l4td;

	snprintf(l4name, 20, ".%s", _name);

	_stack = DDEKit::Thread::stack_slab->alloc();
	if (_stack == 0)
		DDEKit::panic("Thread stack allocation failed.");

	l4td = l4thread_create_long(static_cast<l4thread_t>(L4THREAD_INVALID_ID), DDEKit::Thread_impl::thread_startup,
	                            l4name, _stack + (DDEKit::Thread_impl::STACK_SIZE - 1) * sizeof(l4_addr_t), // XXX ???
                                DDEKit::Thread_impl::STACK_SIZE, L4THREAD_DEFAULT_PRIO,
								this, L4THREAD_CREATE_SYNC);

	if (l4td < 0)
		DDEKit::panic("Thread creation failed.");
}

void DDEKit::Thread_impl::setup_self()
{
	_sleep_cv = DDEKit::Cond_var::create();
	_l4thread = l4thread_myself();
	l4thread_data_set_current(Thread::tls_key_thread, this);
}


DDEKit::Thread* DDEKit::Thread::myself()
{
	return DDEKit::Thread_impl::myself();
}


DDEKit::Thread_impl* DDEKit::Thread_impl::myself()
{
	return static_cast<DDEKit::Thread_impl*>(l4thread_data_get_current(Thread::tls_key_thread));
}

void DDEKit::Thread_impl::thread_startup(void *arg)
{
	DDEKit::Thread_impl *t = reinterpret_cast<DDEKit::Thread_impl*>(arg);
	t->setup_self();
	l4thread_started(0);
	t->execute();
	l4thread_exit();
}


DDEKit::Thread_impl::Thread_impl(char const * const name, void (*func)(void*) = 0,
                                 void *arg = 0)
	: Thread(0,0,""), _data(0), _stack(0), _sleep_cv(0), _func(func), _arg(arg)
{
	int namelen = strlen(name);
	_name = reinterpret_cast<char*>(malloc(namelen));
	strncpy(_name, name, namelen);
	_name[namelen] = 0;
}


DDEKit::Thread_impl::~Thread_impl()
{
	free(_name);

	delete _sleep_cv;

	DDEKit::Thread::stack_slab->free(_stack);
}


void DDEKit::Thread_impl::do_sleep_on_lock(DDEKit::Lock const & l) const
{
	_sleep_cv->wait(l);
}


void DDEKit::Thread_impl::wakeup() const
{
	_sleep_cv->signal();
}


void DDEKit::Thread::sleep_on_lock(DDEKit::Lock const & l)
{
	DDEKit::Thread_impl *t = static_cast<DDEKit::Thread_impl*>(DDEKit::Thread::myself());
	t->do_sleep_on_lock(l);
}


int DDEKit::Thread_impl::id() const
{
	return reinterpret_cast<int>(this->_l4thread);
}

void DDEKit::Thread_impl::terminate()
{
	l4thread_shutdown(_l4thread);
}

void DDEKit::Thread::init_threading() {
	/* register TLS key for pointer to dde thread structure */
	Thread::tls_key_thread = l4thread_data_allocate_key();
	
 	/* create slab for stacks */
	Thread::stack_slab = DDEKit::Slab::create(DDEKit::Thread_impl::STACK_SIZE, 1);

	DDEKit::Thread_impl *me = new DDEKit::Thread_impl("main");
	
	/* setup dde part of thread data */
	me->setup_self();
}

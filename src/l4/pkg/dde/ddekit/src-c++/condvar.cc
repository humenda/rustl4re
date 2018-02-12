/**
 * Unchecked (no BSD invariants) condition variable implementation for
 * dde-internal use. Written from scratch.
 *
 * \author Thomas Friebel <tf13@os.inf.tu-dresden.de>
 * \author Bjoern Doebel <doebel@os.inf.tu-dresden.de>
 */
#include <l4/dde/ddekit/condvar.h>
#include <l4/dde/ddekit/lock.h>
#include <l4/dde/ddekit/memory.h>
#include <l4/dde/ddekit/panic.h>

#include <l4/log/log.h>
#include <l4/util/util.h>
#include <l4/thread/__usem_wrap.h>

#include "internal.h"

#include <cstdlib>

namespace DDEKit
{
	class Cond_var_impl : public Cond_var, public DDEKitObject
	{
		public:
			explicit Cond_var_impl();
			virtual ~Cond_var_impl();

			virtual int wait(DDEKit::Lock const & lock, int const timeout = -1);
			virtual void signal(void);
			virtual void broadcast(void);

		private:
			Cond_var_impl(const Cond_var_impl&) { };
			Cond_var_impl& operator =(const Cond_var_impl&) { return *this; }

			l4_u_semaphore_t *_sem;
			l4_u_semaphore_t *_lock;
			l4_u_semaphore_t *_handshake;
			l4_cap_idx_t     _sem_cap;
			l4_cap_idx_t     _lock_cap;
			l4_cap_idx_t     _handshake_cap;
			l4_addr_t        _sem_base;
			l4_addr_t        _lock_base;
			l4_addr_t        _handshake_base;

			unsigned         _waiters;
			unsigned         _signals;
	};
}

DDEKit::Cond_var *DDEKit::Cond_var::create()
{
	return new Cond_var_impl();
}

DDEKit::Cond_var_impl::Cond_var_impl()
	: _sem(0), _lock(0), _handshake(0),
	  _sem_cap(0), _lock_cap(0), _handshake_cap(0),
	  _sem_base(0), _lock_base(0), _handshake_base(0),
	  _waiters(0), _signals(0)
{
	DDEKit::_create_usem(&_lock, &_lock_cap, &_lock_base, 1);
	DDEKit::_create_usem(&_handshake, &_handshake_cap, &_handshake_base, 0);
	DDEKit::_create_usem(&_sem, &_sem_cap, &_sem_base, 0);
}

DDEKit::Cond_var_impl::~Cond_var_impl()
{
	DDEKit::_delete_usem(_sem_cap, _sem_base);
	DDEKit::_delete_usem(_handshake_cap, _handshake_base);
	DDEKit::_delete_usem(_lock_cap, _lock_base);
}

int DDEKit::Cond_var_impl::wait(DDEKit::Lock const & l, int const timeout)
{
	int rval;

	__lock(_lock, _lock_cap);
	++_waiters;
	__unlock(_lock, _lock_cap);

	l.unlock();

	if (timeout == -1) {
		__lock(_sem, _sem_cap);
		rval = 0;
	} else
		rval = __lock_timed(_sem, _sem_cap, l4util_micros2l4to(timeout * 1000));

	__lock(_lock, _lock_cap);
	if (_signals > 0) {
		/* if we timed out, but there is a signal now, consume it */
		if (rval)
			__lock(_sem, _sem_cap);

		__unlock(_handshake, _handshake_cap);
		--_signals;
	}
	++_waiters;
	__unlock(_lock, _lock_cap);

	l.lock();

	return rval;
}

void DDEKit::Cond_var_impl::signal(void)
{
	__lock(_lock, _lock_cap);

	if (_waiters > _signals) {
		++_signals;
		__unlock(_sem, _sem_cap);
		__unlock(_lock, _lock_cap);
		__lock(_handshake, _handshake_cap);
	} else {
		/* nobody left to wakeup */
		__unlock(_lock, _lock_cap);
	}
}

void DDEKit::Cond_var_impl::broadcast(void)
{
	int waiters;

	__lock(_lock, _lock_cap);

	waiters = _waiters - _signals;
	if (waiters > 0) {
		_signals = _waiters;
		for (int i=0; i<waiters; ++i) {
			__unlock(_sem, _sem_cap);
		}
		__unlock(_lock, _lock_cap);
		for (int i=0; i<waiters; ++i) {
			__lock(_handshake, _handshake_cap);
		}
	} else {
		__unlock(_lock, _lock_cap);
	}
}

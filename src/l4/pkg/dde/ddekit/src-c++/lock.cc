#include <l4/dde/ddekit/lock.h>
#include <l4/dde/ddekit/memory.h>
#include <l4/dde/ddekit/panic.h>
#include <l4/dde/ddekit/printf.h>

#include <l4/util/macros.h>
#include <l4/thread/__usem_wrap.h>
#include <l4/re/c/cap_alloc.h>
#include <cstdlib>

#include "internal.h"

#define DDEKIT_DEBUG_LOCKS 1

namespace DDEKit
{ 
	class Lock_impl : public DDEKit::Lock, public DDEKitObject
	{
		public:

			explicit Lock_impl(const bool init_locked = false);
			virtual ~Lock_impl();

			virtual void lock(void) const
			{
				if (!__lock(_s, _c))
					DDEKit::panic("lock failed");
			}

			virtual void unlock(void) const
			{
				if (!__unlock(_s, _c))
					DDEKit::panic("unlock failed");
			}

			virtual bool trylock(void) const
			{
				return __lock_timed(_s, _c, L4_IPC_TIMEOUT_0) != 0;
			}

		private:
			l4_u_semaphore_t *_s;
			l4_cap_idx_t      _c;
			l4_addr_t         _base;

			Lock_impl(const Lock_impl&) : _s(0), _c(0) { }
			Lock_impl& operator=(const Lock_impl&) { return *this; }
	};
}

DDEKit::Lock *DDEKit::Lock::create(const bool init_locked)
{
	return new DDEKit::Lock_impl(init_locked);
}

DDEKit::Lock_impl::Lock_impl(const bool init_locked) : _s(0), _c(0)
{
	DDEKit::_create_usem(&_s, &_c, &_base, init_locked ? 0 : 1);
}


DDEKit::Lock_impl::~Lock_impl()
{
	__lock_free(_c);
	DDEKit::simple_free(reinterpret_cast<void*>(_base));
}

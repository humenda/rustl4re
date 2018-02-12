#include <l4/dde/ddekit/semaphore.h>
#include <l4/dde/ddekit/memory.h>
#include <l4/dde/ddekit/panic.h>
#include <l4/dde/ddekit/printf.h>

#include <l4/thread/__usem_wrap.h>
#include <l4/util/util.h>
#include <cstdlib>

#include "internal.h"

namespace DDEKit
{
	class Semaphore_impl : public DDEKit::Semaphore, public DDEKitObject
	{
		public:
			explicit Semaphore_impl(int const value);
			~Semaphore_impl();

			virtual bool down(int const timeout = -1);
			virtual bool trydown(void);
			virtual void up(void);

		private:
			l4_u_semaphore_t *_sem;
			l4_cap_idx_t     _cap;
			l4_addr_t        _base;

	};
}

DDEKit::Semaphore* DDEKit::Semaphore::create(int const value)
{
	return new Semaphore_impl(value);
}


DDEKit::Semaphore_impl::Semaphore_impl(int const value)
	: _sem(0), _cap(0), _base(0)
{
	DDEKit::_create_usem(&_sem, &_cap, &_base, value);
}


DDEKit::Semaphore_impl::~Semaphore_impl()
{
	if (!__lock_free(_cap))
		DDEKit::panic("ddekit_sem_deinit failed");
	DDEKit::simple_free(reinterpret_cast<void*>(_base));
}


bool DDEKit::Semaphore_impl::down(int const timeout)
{
	if (timeout == -1)
		return __lock(_sem, _cap) == 1;
	else
		return __lock_timed(_sem, _cap, l4util_micros2l4to(timeout * 1000)) == 1;
}

bool DDEKit::Semaphore_impl::trydown()
{
	int r = __lock_timed(_sem, _cap, L4_IPC_TIMEOUT_0) == 1;
	return r;
}


void DDEKit::Semaphore_impl::up(void)
{
	if (!__unlock(_sem, _cap))
		DDEKit::panic("unlock failed");
}

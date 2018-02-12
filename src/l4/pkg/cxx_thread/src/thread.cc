/*
 * (c) 2004-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU Lesser General Public License 2.1.
 * Please see the COPYING-LGPL-2.1 file for details.
 */

#include <l4/cxx/thread>
#include <l4/cxx/iostream>
#include <l4/cxx/l4iostream>

#include <l4/sys/thread>
#include <l4/sys/factory>
#include <l4/sys/scheduler>
#include <l4/sys/ipc.h>

#include <l4/util/util.h>
#include <l4/re/env>

namespace cxx {

  namespace {
    l4_cap_idx_t _next_cap = 20 << L4_CAP_SHIFT;
    static inline l4_cap_idx_t alloc_next_cap()
    { l4_cap_idx_t r = _next_cap; _next_cap += (1UL << L4_CAP_SHIFT); return r; }
    l4_addr_t _next_free_utcb;
    l4_addr_t _utcb_base;
  };

  L4::Cap<void> Thread::_pager;
  L4::Cap<void> Thread::_master;

  Thread::Thread( bool /*initiate*/ )
    : _cap(L4Re::Env::env()->main_thread()), _state(Running)
  {
    _next_free_utcb = L4Re::Env::env()->first_free_utcb();
    _master = _cap;
    L4::Thread::Attr attr;
    _cap->control(attr);
    _pager = attr.pager();
    _utcb_base = l4_addr_t(l4_utcb());
  }

  int
  Thread::create()
  {
    l4_msgtag_t err = L4Re::Env::env()->factory()->create(_cap);
    if (l4_msgtag_has_error(err) || l4_msgtag_label(err) < 0)
      return l4_msgtag_label(err);

    L4::Thread::Attr attr(l4_utcb());
    attr.pager(_pager);
    attr.bind((l4_utcb_t*)_next_free_utcb, L4Re::This_task);
    _next_free_utcb += L4_UTCB_OFFSET;
    return _cap->control(attr).label();
  }

  Thread::Thread( void *stack )
    : _cap(alloc_next_cap()), _state(Dead), _stack(stack)
  {
    create();
  }

  Thread::Thread( void *stack, L4::Cap<L4::Thread> const &cap )
    : _cap(cap), _state(Dead), _stack(stack)
  {
    create();
  }

  void Thread::start()
  {
    if(_cap == _master) {
      _state = Running;
      run();
    } else {

      L4Re::Env::env()->scheduler()->
	    run_thread(_cap, l4_sched_param(0xff, 0));

      *(--((l4_umword_t*&)_stack)) = (l4_umword_t)this;
      *(--((l4_umword_t*&)_stack)) = 0;
      _cap->ex_regs((l4_umword_t)start_cxx_thread,
                    (l4_umword_t)_stack, 0);

      if (l4_msgtag_has_error(l4_ipc_send(_cap.cap(), l4_utcb(),
	                                  l4_msgtag(0,0,0,0),
                                          L4_IPC_NEVER)))
        L4::cerr << "ERROR: (master) error while thread handshake: "
                 << _master << "->" << self() << "\n";
    }
  }

  void Thread::execute()
  {
    l4_umword_t src;
    l4_msgtag_t tag = l4_ipc_wait(l4_utcb(), &src, L4_IPC_NEVER);

    if (l4_msgtag_has_error(tag))
      L4::cerr << "ERROR: (slave) error while thread handshake: "
               << self() << "\n";
    _state = Running;
    run();
    shutdown();
  };

  void Thread::shutdown()
  {
    _state = Stopped;
    for (;;)
      l4_ipc_sleep(L4_IPC_NEVER);
  }

  void Thread::stop()
  {
#if 0
    L4::cerr << "~Thread[" << self() << "]() called from "
             << l4_myself() << " @" << L4::hex
             << __builtin_return_address(0) << "\n";
#endif
    if(_cap != _master)
      {
	*(((l4_umword_t*&)_stack)--) = (l4_umword_t)this;
        _cap->ex_regs((l4_umword_t)kill_cxx_thread,
                      (l4_umword_t)_stack, 0);
      }
  }

  Thread::~Thread()
  { stop(); }

};

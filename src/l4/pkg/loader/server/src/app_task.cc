/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include "app_task.h"
//#include "slab_alloc.h"
//#include "globals.h"
#include <l4/re/error_helper>
#include <l4/re/util/cap_alloc>

#include <l4/cxx/iostream>

#include "obj_reg.h"


using L4Re::Util::cap_alloc;
using L4Re::Dataspace;

#if 0
static Slab_alloc<App_task> *alloc()
{
  static Slab_alloc<App_task> a;
  return &a;
}
#endif

#if 0
void *App_task::operator new (size_t) throw()
{ return alloc()->alloc(); }

void App_task::operator delete (void *m) throw()
{ alloc()->free((App_task*)m); }
#endif

int
App_task::op_signal(L4Re::Parent::Rights, unsigned long sig, unsigned long val)
{
  switch (sig)
    {
    case 0: // exit
        {
          L4Re::Util::cap_alloc.free(obj_cap());
          delete this;
          if (val != 0)
            L4::cout << "LDR: task " << this << " exited with " << val
                     << '\n';
          return -L4_ENOREPLY;
        }
    default: break;
    }
  return L4_EOK;
}

App_task::App_task()
  : _task(L4::Cap<L4::Task>::Invalid),
    _thread(L4::Cap<L4::Thread>::Invalid)
{
  //object_pool.cap_alloc()->alloc(&_alloc);
  Gate_alloc::registry.register_obj(&_rm);
  _rm.init();
  //object_pool.cap_alloc()->alloc(&log);
}

App_task::~App_task()
{
  //object_pool.cap_alloc()->free(&_alloc);
  Gate_alloc::registry.unregister_obj(&_rm);
  //object_pool.cap_alloc()->free(&log);
  if (_thread.is_valid())
    cap_alloc.free(_thread);

  if (_task.is_valid())
    cap_alloc.free(_task);

  if (_sched.obj_cap().is_valid())
    cap_alloc.free(_sched.obj_cap());

  Gate_alloc::registry.unregister_obj(this);
}

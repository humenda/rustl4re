/*
 * (c) 2008-2009 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include <l4/re/log>
#include <l4/re/env>
#include "region.h"
#include "sched_proxy.h"

#include <l4/sys/cxx/ipc_epiface>

class App_task : public L4::Epiface_t<App_task, L4Re::Parent>
{
private:
  L4::Cap<L4::Task> _task;
  L4::Cap<L4::Thread> _thread;
  L4::Cap<L4::Log> _log;

  Region_map _rm;

public:
  Sched_proxy _sched;

  App_task();
  static L4::Cap<L4Re::Mem_alloc> allocator()
  { return L4Re::Env::env()->mem_alloc(); }

  int op_signal(L4Re::Parent::Rights, unsigned long sig, unsigned long val);

  Region_map *rm() { return &_rm; }

  void task_cap(L4::Cap<L4::Task> const &c) { _task = c; }
  void thread_cap(L4::Cap<L4::Thread> const &c) { _thread = c; }

  L4::Cap<L4::Task> task_cap() const { return _task; }
  L4::Cap<L4::Thread> thread_cap() const { return _thread; }

  virtual ~App_task();
};

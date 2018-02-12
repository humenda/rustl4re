/*
 * (c) 2008-2009 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include <l4/re/mem_alloc>
#include <l4/sys/cxx/ipc_epiface>
#include <l4/cxx/list>

class Allocator : public L4::Epiface_t<Allocator, L4Re::Mem_alloc>
{
private:
  long _sched_prio_limit;
  l4_umword_t _sched_cpu_mask;

public:
  explicit Allocator(unsigned prio_limit = 0)
  : _sched_prio_limit(prio_limit), _sched_cpu_mask(~0UL)
  {}

  virtual ~Allocator();

  int op_create(L4::Factory::Rights r, L4::Ipc::Cap<void> &res,
                long type, L4::Ipc::Varg_list<> &&args);

  static Allocator *root_allocator();
};

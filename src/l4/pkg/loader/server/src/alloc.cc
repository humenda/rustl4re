/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/cxx/exceptions>
#include <l4/cxx/auto_ptr>
#include <l4/cxx/l4iostream>

#include <l4/sys/factory>
#include <l4/re/util/meta>

#include <cstdlib>

#include "obj_reg.h"
#include "alloc.h"
//#include "globals.h"
//#include "slab_alloc.h"
#include "region.h"
#include "name_space.h"
#include "log.h"
#include "sched_proxy.h"



Allocator::~Allocator()
{
}



class LLog : public Ldr::Log
{
public:
  LLog(char const *t, int l, unsigned char col) : Log()
  {
    if (l > 32)
      l = 32;

    set_tag(cxx::String(t, l));
    set_color(col);
  }

  virtual ~LLog() {}
};

static L4::Ipc::Cap<void> new_cap(L4::Epiface *o)
{
  Gate_alloc::registry.register_obj(o);
  return L4::Ipc::make_cap(o->obj_cap(), L4_CAP_FPAGE_RWSD);
}

int
Allocator::op_create(L4::Factory::Rights, L4::Ipc::Cap<void> &res,
                     long type, L4::Ipc::Varg_list<> &&args)
{
  switch (type)
    {
    case L4Re::Namespace::Protocol:
      res = new_cap(new Ldr::Name_space());
      return L4_EOK;

    case L4Re::Rm::Protocol:
      res = new_cap(new Region_map());
      return L4_EOK;

    case L4_PROTO_LOG:
        {
          L4::Ipc::Varg tag = args.next();

          if (!tag.is_of<char const *>())
            return -L4_EINVAL;

          L4::Ipc::Varg col = args.next();

          int color;
          if (col.is_of<char const *>())
            color = LLog::color_value(cxx::String(col.value<char const*>(),
                  col.length()));
          else if(col.is_of_int()) // ignore sign
            color = col.value<long>();
          else
            color = 7;

          Ldr::Log *l = new LLog(tag.value<char const*>(), tag.length(), color);

          res = new_cap(l);
          return L4_EOK;
        }

    case L4::Scheduler::Protocol:
        {
          if (!_sched_prio_limit)
            return -L4_ENODEV;

          L4::Ipc::Varg p_max = args.next(), p_base = args.next(), cpus = args.next();

          if (!p_max.is_of_int() || !p_base.is_of_int()) // ignore sign
            return -L4_EINVAL;

          if (p_max.value<l4_mword_t>() > _sched_prio_limit
              || p_base.value<l4_mword_t>() > _sched_prio_limit)
            return -L4_ERANGE;

          if (p_max.value<l4_umword_t>() <= p_base.value<l4_umword_t>())
            return -L4_EINVAL;

          l4_umword_t cpu_mask = ~0UL;

          if (!cpus.is_of<void>() && cpus.is_of_int())
            cpu_mask = cpus.value<l4_umword_t>();

          Sched_proxy *o = new Sched_proxy();
          o->set_prio(p_base.value<l4_mword_t>(), p_max.value<l4_mword_t>());
          o->restrict_cpus(cpu_mask);

          res = new_cap(o);
          return L4_EOK;
        }

    default:
      return -L4_ENODEV;
    }
}

Allocator *
Allocator::root_allocator()
{
  static Allocator ra(300000);
  return &ra;
}

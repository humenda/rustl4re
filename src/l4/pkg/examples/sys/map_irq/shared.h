#pragma once

#include <l4/sys/capability>
#include <l4/sys/cxx/ipc_iface>
#include <l4/sys/cxx/ipc_types>


struct Irq_source :
  L4::Kobject_t<Irq_source, L4::Kobject, 0x77, L4::Type_info::Demand_t<1> >
{
  L4_INLINE_RPC(int, map_irq, (L4::Ipc::Cap<L4::Irq> irq));
  L4_INLINE_RPC(int, start, (unsigned nrs));
  typedef L4::Typeid::Rpcs<map_irq_t, start_t> Rpcs;
};

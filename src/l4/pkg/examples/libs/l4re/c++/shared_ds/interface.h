#pragma once

#include <l4/sys/capability>
#include <l4/re/dataspace>
#include <l4/cxx/ipc_stream>

/**
 * Interface class for remote object.
 *
 * Inherits from L4::Kobject, via the L4::Kobject_t helper
 * template that generates the dynamic type information for the meta
 * protocol.
 */
class My_interface : public L4::Kobject_t<My_interface, L4::Kobject>
{
  // Disable instantiation and copy, because we just work via
  // L4::Cap<...> references.
  L4_KOBJECT(My_interface)

public:
  /**
   * The single method of our object.
   * Requesting the data space and the IRQ to notify the server.
   */
  int get_shared_buffer(L4::Cap<L4Re::Dataspace> ds, L4::Cap<L4::Irq> irq) throw();
};


inline
int
My_interface::get_shared_buffer(L4::Cap<L4Re::Dataspace> ds, L4::Cap<L4::Irq> irq) throw()
{
  L4::Ipc::Iostream s(l4_utcb());
  // we have just a single operation, so no opcode needed
  // s << Opcode;

  // put receive buffer for data-space cap and the irq cap into the stream
  s << L4::Ipc::Small_buf(ds)
    << L4::Ipc::Small_buf(irq);

  return l4_error(s.call(cap()));
}

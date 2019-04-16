#include <l4/re/dataspace>
#include <l4/sys/capability>
#include <l4/sys/cxx/ipc_iface>

using L4::Ipc::Cap;
using L4Re::Dataspace;

struct Shm: L4::Kobject_t<Shm, L4::Kobject, 66, L4::Type_info::Demand_t<1>> {
  L4_INLINE_RPC(int, witter, (l4_uint32_t length, Cap<Dataspace> ds));
  typedef L4::Typeid::Rpcs<witter_t> Rpcs;
};

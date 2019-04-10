#include <l4/sys/cxx/ipc_iface>
#include <l4/re/dataspace>
#include <l4/sys/cxx/ipc_string>
#include <l4/sys/capability>

using L4::Ipc::String;
using L4::Ipc::Cap;
using L4Re::Dataspace;

struct Bencher: L4::Kobject_t<Bencher, L4::Kobject, 50159747054, L4::Type_info::Demand_t<1>> {
  L4_INLINE_RPC(int, sub, (l4_uint32_t a, l4_uint32_t b, l4_int32_t& c));
  L4_INLINE_RPC(int, strpingpong, (String<> input, String<char>& a));
  L4_INLINE_RPC(int, check_dataspace, (l4_uint64_t size, Cap<Dataspace> ds));
  typedef L4::Typeid::Rpcs<sub_t, strpingpong_t, check_dataspace_t> Rpcs;
};

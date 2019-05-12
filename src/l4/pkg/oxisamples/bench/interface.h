#include <l4/re/dataspace>
#include <l4/sys/capability>
#include <l4/sys/cxx/ipc_iface>
#include <l4/sys/cxx/ipc_string>

using L4::Ipc::Cap;
using L4::Ipc::String;
using L4Re::Dataspace;

struct Bencher: L4::Kobject_t<Bencher, L4::Kobject, 50159747054, L4::Type_info::Demand_t<1>> {
  L4_INLINE_RPC(int, sub, (l4_umword_t a, l4_umword_t b, l4_int32_t& c));
  L4_INLINE_RPC(int, str_ping_pong, (String<> input, String<char>& a));
  L4_INLINE_RPC(int, string_ping_pong, (String<> input, String<char>& a));
  L4_INLINE_RPC(int, map_cap, (Cap<Dataspace> ds));
  typedef L4::Typeid::Rpcs<sub_t, str_ping_pong_t, string_ping_pong_t, map_cap_t> Rpcs;
};

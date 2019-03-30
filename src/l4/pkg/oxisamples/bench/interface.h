#include <l4/sys/cxx/ipc_string>
#include <l4/sys/capability>
#include <l4/sys/cxx/ipc_iface>

using L4::Ipc::String;

struct Bencher: L4::Kobject_t<Bencher, L4::Kobject, 3134984174> {
  L4_INLINE_RPC(int, sub, (l4_uint32_t a, l4_uint32_t b, l4_int32_t *c));
  L4_INLINE_RPC(int, strpingpong, (String<> input, L4::Ipc::String<char> &a));
  typedef L4::Typeid::Rpcs<sub_t, strpingpong_t> Rpcs;
};

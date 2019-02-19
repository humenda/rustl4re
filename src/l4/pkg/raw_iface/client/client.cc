#include <cstring>
#include <l4/re/env>
#include <l4/sys/capability>
#include <l4/sys/capability>
#include <l4/sys/cxx/ipc_iface>
#include <l4/sys/cxx/ipc_iface>
#include <l4/sys/cxx/ipc_string>
#include <l4/sys/err.h>
#include <l4/sys/types.h>
#include <stdio.h>

using L4::Ipc::String;

struct StrTest : L4::Kobject_t<StrTest, L4::Kobject, 0x8080> {
  L4_INLINE_RPC(int, echo, (String<> s, l4_uint32_t *res));
  typedef L4::Typeid::Rpcs<echo_t> Rpcs;
};

int main() {
    L4::Cap<StrTest> channel = L4Re::Env::env()->get_cap<StrTest>("channel");
    if (!channel.is_valid()) {
        printf("Could not get server capability!\n");
        throw 0;
    }

    printf("Invoking echo('Hello world!')\n");
    l4_uint32_t *res;
    channel->echo(String<>("Hello, world!"), res);
    printf("done\n");
    return 0;
}


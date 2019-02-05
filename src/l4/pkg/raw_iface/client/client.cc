#include <cstring>
#include <l4/re/env>
#include <l4/re/util/cap_alloc>
#include <l4/sys/err.h>
#include <l4/sys/types.h>
#include <stdio.h>
#include <l4/sys/capability>
#include <l4/sys/cxx/ipc_iface>
#include <l4/re/dataspace>

using L4Re::Dataspace;

int main() {
    L4::Cap<Dataspace> channel = L4Re::Env::env()->get_cap<Dataspace>("channel");
    if (!channel.is_valid()) {
        printf("Could not get server capability!\n");
        throw 0;
    }

    printf("Invoking Cap<Dataspace>::clear()\n");
    channel->clear(0xff, 0xff);
    printf("allocating blah\n");
    channel->allocate(0xff, 0xff);
    return 0;
}


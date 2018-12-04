#include <l4/syint/utcb.h>
#include <l4/sys/cxx/ipc_basics>
#include <l4/sys/cxx/ipc_types>
#include <l4/sys/err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace L4::Ipc::Msg;

/// Use the serialisation functionality from ipc_basic to write long, a float
/// and a bool.
extern "C" char *write_long_float_bool() {
    int limit = 1024;
    char *v = (char *)malloc(limit);
    memset(v, 0, limit);
    unsigned long f = 987654;
    float g = 3.14;
    bool metal = true;
    int offset = msg_add(v, 0, limit, f);
    offset = msg_add(v, offset, limit, g);
    msg_add(v, offset, limit, metal);
    return v;
}

/// Use the serialisation functionality from ipc_basic to write a bool, a long
//and a float
extern "C" char *write_bool_long_float() {
    int limit = 1024;
    char *v = (char *)malloc(limit);
    memset(v, 0, limit);
    unsigned long f = 987654;
    float g = 3.14;
    bool metal = true;
    int offset = msg_add(v, 0, limit, metal);
    offset = msg_add(v, offset, limit, f);
    msg_add(v, offset, limit, g);
    return v;
}

extern "C" char *write_u64_u32_u64() {
    int limit = 1024;
    char *v = (char *)malloc(limit);
    memset(v, 0, limit);
    l4_uint64_t a = 42;
    l4_uint32_t b = 1337;
    int offset = msg_add(v, 0, limit, a);
    offset = msg_add(v, offset, limit, b);
    msg_add(v, offset, limit, a);
    return v;
}

extern "C" void write_cxx_snd_fpage(char *v, l4_fpage_t fp) {
    auto sfp = L4::Ipc::Snd_fpage(fp);
    msg_add(v, 0, 1024, sfp);
}
extern "C" char *write_u32_and_fpage() {
    char *v = (char *)malloc(1024);
    memset(v, 0, 1024);
    int offset = msg_add(v, 0, 1024, (l4_uint32_t)314);
    // wriute fpage
    auto fp = l4_obj_fpage(0xcafebabe, 0, L4_FPAGE_RWX);
    auto sfp = L4::Ipc::Snd_fpage(fp);
    msg_add(v, offset, 1024, sfp);
    return v;
}

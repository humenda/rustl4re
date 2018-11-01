#include <l4/syint/utcb.h>
#include <l4/sys/cxx/ipc_basics>
#include <l4/sys/err.h>
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

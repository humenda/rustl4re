#include <l4/sys/debugger.h>
#include <l4/l4re-rust/debug.h>

l4_msgtag_t l4_debugger_set_object_name_w(unsigned long cap, const char *name) {
    return l4_debugger_set_object_name(cap, name);
}

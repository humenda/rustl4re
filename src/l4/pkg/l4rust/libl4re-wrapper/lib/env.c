// Provide wrapper functions for all inlined functions from l4/re/env.h

#include <l4/l4rust/env.h>
#include <l4/re/env.h>

EXTERN l4_cap_idx_t l4re_env_get_cap_w(char const *name) L4_NOTHROW {
    return l4re_env_get_cap(name);
}


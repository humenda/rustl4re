#include <l4/syint/l4sys.h>

extern "C" unsigned l4_is_invalid_cap_w(l4_cap_idx_t cap) {
    return l4_is_invalid_cap(cap);
}

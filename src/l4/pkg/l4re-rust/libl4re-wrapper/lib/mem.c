#include <l4/l4re-rust/mem.h>
#include <l4/re/c/mem_alloc.h>

long l4re_ma_alloc_w(long size, l4re_ds_t const mem, unsigned long flags) {
    return l4re_ma_alloc(size, mem, flags);
}

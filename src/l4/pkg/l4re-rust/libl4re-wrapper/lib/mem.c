#include <l4/l4re-rust/mem.h>
#include <l4/re/c/mem_alloc.h>
#include <l4/re/c/rm.h>

EXTERN long l4re_ma_alloc_w(long size, l4re_ds_t const mem, unsigned long flags) {
    return l4re_ma_alloc(size, mem, flags);
}

EXTERN int l4re_rm_detach_ds_w(void *addr, l4re_ds_t *ds) {
    return l4re_rm_detach_ds(addr, ds);
}

EXTERN int l4re_rm_detach_w(void *addr) {
    return l4re_rm_detach(addr);
}



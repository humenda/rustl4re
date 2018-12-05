#include <l4/re/c/mem_alloc.h>

#ifdef __cplusplus
#define EXTERN extern "C"
#else
#define EXTERN
#endif

EXTERN long l4re_ma_alloc_w(long size, l4re_ds_t const mem, unsigned long flags);

EXTERN int l4re_rm_detach_ds_w(void *addr, l4re_ds_t *ds);

EXTERN int l4re_rm_detach_w(void *addr);



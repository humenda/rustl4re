#pragma once
#include <l4/sys/compiler.h>
#include <l4/sys/types.h>

EXTERN_C_BEGIN
/* Configure counters -> setup is in lib/perfcnt/perfcnt.c */
void perfconf(void);
/* Read and print counters */
void perfread(void);
void perfread2(l4_uint64_t *v1,l4_uint64_t *v2,l4_uint64_t *v3,l4_uint64_t *v4);

EXTERN_C_END

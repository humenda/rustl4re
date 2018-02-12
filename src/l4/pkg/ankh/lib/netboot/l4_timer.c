#include <l4/re/env.h>
#include <l4/sys/types.h>
#include "timer.h"

unsigned long currticks(void)
{
    l4_cpu_time_t t = l4re_kip()->clock;
    return (unsigned long)(t >> 10);
}

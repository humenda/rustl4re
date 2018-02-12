#pragma once

#if 0
#include "l4/util/util.h"

inline void udelay(unsigned us)
{
  l4_usleep(us);
}
#else

// environment using this library needs to supply a udelay function
void udelay(unsigned us);

#endif

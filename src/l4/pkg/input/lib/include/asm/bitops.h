#ifndef _I386_BITOPS_H
#define _I386_BITOPS_H

#include <l4/util/bitops.h>

#define set_bit(nr, addr) \
	l4util_set_bit((nr), (addr))
#define clear_bit(nr, addr) \
	l4util_clear_bit((nr), (addr))
#define change_bit(nr, addr) \
	l4util_complement_bit((nr),(addr))
#define test_bit(nr, addr) \
	l4util_test_bit((nr),(addr))

#endif


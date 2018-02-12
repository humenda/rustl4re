/*
 * Legacy functions.
 *
 * WARNING: Do not use this functions in L4Linux!
 *
 */

#if 0
#include <asm/l4lxapi/misc.h>

/*
 * Some libs/code from the DROPS tree which is linked to L4Linux
 * needs l4_sleep...
 */
L4_CV void l4_sleep(int ms)
{
	l4lx_sleep(ms);
}
#endif

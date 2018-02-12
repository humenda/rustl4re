#ifndef __DDE_26_INPUT_H
#define __DDE_26_INPUT_H

#include <l4/input/libinput.h>

/** event callback function */
typedef L4_CV void (*linux_ev_callback)(struct l4input *);

extern linux_ev_callback l4dde26_ev_callback;

/** Register event callback function.
 *
 * \param cb   new callback function
 * \return     old callback function pointer
 */
linux_ev_callback l4dde26_register_ev_callback(linux_ev_callback cb);


/** Run callback function.
 */
static inline int l4dde26_do_ev_callback(struct l4input *e)
{
	if (l4dde26_ev_callback)
		l4dde26_ev_callback(e);

	return 0;
}

#endif

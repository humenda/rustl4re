#include "dde26_input.h"

#include <linux/kernel.h>

/* Callback function to be called if an input event arrives and needs to
 * be handled
 */
linux_ev_callback l4dde26_ev_callback = NULL;

/* Register an input event callback function.
 *
 * \return pointer to old callback function
 */
linux_ev_callback l4dde26_register_ev_callback(linux_ev_callback cb)
{
	linux_ev_callback old = l4dde26_ev_callback;
	l4dde26_ev_callback = cb;

	return old;
}


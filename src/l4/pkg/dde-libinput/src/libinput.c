#include <l4/dde/linux26/dde26.h>
#include <l4/sys/types.h>

#include <linux/serio.h>
#include "dde26_input.h"

extern int dde_evdev_open(int i);

L4_CV int l4input_pcspkr(int tone)
{
  return 0;
}

L4_CV int l4input_ispending(void)
{
  return 0;
}

L4_CV int l4input_flush(void *buffer, int count)
{
  return 0;
}

L4_CV int l4input_init(int mode, linux_ev_callback cb)
{
	l4dde26_register_ev_callback(cb);

	printk("DDE-Input is ready.\n");

	return 0;
}

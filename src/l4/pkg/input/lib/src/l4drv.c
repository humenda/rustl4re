/*
 * (c) 2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *          Torsten Frenzel <frenzel@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/serio.h>
#include <linux/workqueue.h>

#include <l4/input/drv_reg.h>
#include <l4/sys/err.h>
#include <l4/re/env.h>

#include <stdio.h>

#define DRIVER_DESC	"L4 driver"

MODULE_AUTHOR("Torsten Frenzel");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");


struct l4drv_data {

    char name[64];
    char phys[32];
    struct input_dev dev;

    unsigned char enabled;

    void *private;
};


static void input_handler(Input_event event, void *private)
{
  struct input_dev *dev = &((struct l4drv_data *)private)->dev;

  input_event(dev, event.type, event.code, event.value);
  input_sync(dev);
}

/* XXX this is just a dummy */
static irqreturn_t l4drv_interrupt(struct serio *serio, unsigned char data,
			            unsigned int flags, struct pt_regs *regs)
{
  return IRQ_HANDLED;
}


static int l4drv_event(struct input_dev *dev, unsigned int type, unsigned int code, int value)
{
  return -1;
}

static void l4drv_set_device_attrs(struct l4drv_data *data)
{
  memset(&data->dev, 0, sizeof(struct input_dev));

  init_input_dev(&data->dev);

  data->dev.name = data->name;
  data->dev.phys = data->phys;
  data->dev.id.bustype = 0;
  data->dev.id.vendor = 0x1;
  data->dev.id.product = 0x1;
  data->dev.id.version = 0x1;
  data->dev.event = l4drv_event;
  data->dev.private = data;
  data->dev.dev = 0;
  // FIXME: this information should come from the driver!
  data->dev.evbit[0] = BIT(EV_KEY)|BIT(EV_ABS);
  set_bit(BTN_LEFT, data->dev.keybit);
  set_bit(KEY_1, data->dev.keybit);
  set_bit(KEY_2, data->dev.keybit);
  set_bit(KEY_3, data->dev.keybit);
  set_bit(KEY_4, data->dev.keybit);
  set_bit(KEY_5, data->dev.keybit);
  set_bit(KEY_6, data->dev.keybit);
  set_bit(KEY_7, data->dev.keybit);
  set_bit(KEY_8, data->dev.keybit);
  set_bit(KEY_9, data->dev.keybit);
  set_bit(KEY_0, data->dev.keybit);
  set_bit(KEY_A, data->dev.keybit);
  set_bit(KEY_B, data->dev.keybit);
  set_bit(KEY_C, data->dev.keybit);
  set_bit(KEY_D, data->dev.keybit);
  set_bit(KEY_E, data->dev.keybit);
  set_bit(KEY_F, data->dev.keybit);
  set_bit(KEY_ENTER, data->dev.keybit);
}

static inline void l4drv_enable(struct l4drv_data *data)
{
}

static inline void l4drv_disable(struct l4drv_data *data)
{
}

static int l4drv_probe(struct l4drv_data *data)
{
  struct arm_input_ops *input_ops;

  if (!(input_ops = arm_input_probe(data->name)))
    {
      printk(KERN_INFO "l4drv: Could not find driver for %s.\n", data->name);
      return 1;
    }
 
  input_ops->enable();
  input_ops->attach(input_handler, data);
  data->private = input_ops;

  return 0;
}


static void l4drv_connect(struct serio *serio, struct serio_driver *drv)
{
  struct l4drv_data *data;

  if (!(data = kmalloc(sizeof(struct l4drv_data), GFP_KERNEL)))
    return;
  memset(data, 0, sizeof(struct l4drv_data));

  serio->private = data;

  if (serio_open(serio, drv)) {
      kfree(data);
      return;
  }

  strcpy(data->name, serio->name);
  sprintf(data->phys, "%s/input0", serio->phys);
  l4drv_set_device_attrs(data);
  data->enabled = 1;

  if (l4drv_probe(data)) {
      serio_close(serio);
      serio->private = NULL;
      kfree(data);
      return;
  }

  input_register_device(&data->dev);

  printk(KERN_INFO "l4drv: %s on %s\n", data->name, serio->phys);
}

static int l4drv_reconnect(struct serio *serio)
{
  struct l4drv_data *data = serio->private;
  struct serio_driver *drv = serio->drv;

  if (!data || !drv) {
      printk(KERN_DEBUG "l4drv: reconnect request, but serio is disconnected, ignoring...\n");
      return -1;
  }

  l4drv_disable(data);
  l4drv_enable(data);

  return 0;
}

static void l4drv_disconnect(struct serio *serio)
{
  struct l4drv_data *data = serio->private;

  l4drv_disable(data);

  input_unregister_device(&data->dev);
  serio_close(serio);
  kfree(data);
}

static void l4drv_cleanup(struct serio *serio)
{}


static struct serio_driver l4drv = {
	.driver		= {
		.name	= "l4drv",
	},
	.description	= DRIVER_DESC,
	.interrupt	= l4drv_interrupt,
	.connect	= l4drv_connect,
	.reconnect	= l4drv_reconnect,
	.disconnect	= l4drv_disconnect,
	.cleanup	= l4drv_cleanup,
};


int __init l4drv_init(void)
{
  serio_register_driver(&l4drv);
  return 0;
}

void __exit l4drv_exit(void)
{
  serio_unregister_driver(&l4drv);
}

module_init(l4drv_init);
module_exit(l4drv_exit);

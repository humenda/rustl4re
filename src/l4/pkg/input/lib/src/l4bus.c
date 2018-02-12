/*
 * (c) 2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *          Torsten Frenzel <frenzel@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/serio.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/err.h>

#include <l4/io/io.h>

struct l4bus_port {
    struct serio	*io;
    unsigned int	irq;
};

#if 0
static irqreturn_t l4bus_int(int irq, void *dev_id, struct pt_regs *regs)
{
  struct l4bus_port *port = dev_id;
  int handled = IRQ_NONE;

  (void)port;
  return handled;
}
#endif

static int l4bus_write(struct serio *io, unsigned char val)
{
  struct l4bus_port *port = io->port_data;
  (void)port;

  return 0;
}

static int l4bus_open(struct serio *io)
{
  struct l4bus_port *port = io->port_data;
  (void)port;

  return 0;
}

static void l4bus_close(struct serio *io)
{
  //struct l4bus_port *port = io->port_data;
}

static int l4bus_probe(const char *name)
{
  struct l4bus_port *port;
  struct serio *io;
  int ret;

  port = kmalloc(sizeof(struct l4bus_port), GFP_KERNEL);
  io = kmalloc(sizeof(struct serio), GFP_KERNEL);
  if (!port || !io) {
      ret = -ENOMEM;
      goto out;
  }

  memset(port, 0, sizeof(struct l4bus_port));
  memset(io, 0, sizeof(struct serio));

  io->type	= SERIO_L4;
  io->write	= l4bus_write;
  //io->read	= 0;
  io->open	= l4bus_open;
  io->close	= l4bus_close;
  strlcpy(io->name, name, sizeof(io->name));
  strlcpy(io->phys, name, sizeof(io->phys));
  io->port_data   = port;
  //io->dev.parent	= &dev->dev;

  port->io	= io;
  port->irq	= -1;

  serio_register_port(io);
  return 0;

out:
  kfree(io);
  kfree(port);
  return ret;
}

static int __init l4bus_init(void)
{
  // FIXME: iterate over vbus to find devices
  // ... indeed: disabled those two probe functions because they emit a
  // warning and someone might think there could be working omap input stuff
  if (0)
    {
      l4bus_probe("OMAP_TSC");
      l4bus_probe("OMAP_KP");
    }
  return 0;
}

module_init(l4bus_init);

MODULE_AUTHOR("Torsten Frenzel");
MODULE_DESCRIPTION("L4 bus driver");
MODULE_LICENSE("GPL");

/*
 * Inputlib for Fiasco-UX implementing l4/input/libinput.h
 *
 * by Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *
 * Some code is taken from l4/pkg/input and l4/pkg/con
 *
 * Adaptions by Christian Helmuth <ch12@os.inf.tu-dresden.de>
 */
/*
 * (c) 2007-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>,
 *               Christian Helmuth <ch12@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <stdlib.h>

#include <l4/input/libinput.h>
#include <l4/sys/err.h>
#include <l4/io/io.h>
#include <l4/re/env.h>

#include <linux/interrupt.h>
#include <linux/input.h>

#include <stdio.h>
#include <string.h>

#include "internal.h"

#define Panic(a...) do { fprintf(stderr, a); exit(1); } while (0)
/* needs to be the same as in ux_con! */
enum {
  INPUTMEM_SIZE = 1 << 12,
  NR_INPUT_OBJS = INPUTMEM_SIZE / sizeof(struct l4input),
};

static int input_pos;
static struct l4input *input_mem;
static L4_CV void (*input_handler)(struct l4input *);
static struct input_dev idev;


static int dequeue_event(struct l4input *ev)
{
  struct l4input *i = input_mem + input_pos;

  if (i->time == 0)
    return 0;

  i->stream_id = (unsigned long)&idev;
  *ev = *i;

  i->time = 0;

  input_pos++;
  if (input_pos == NR_INPUT_OBJS)
    input_pos = 0;

  return 1;
}

static irqreturn_t irq_handler(int irq,
                               void *dev_id, struct pt_regs *regs)
{
  struct l4input ev;

  while (dequeue_event(&ev))
    input_handler(&ev);

  return 0;
}

static void *map_inputmemory(l4_addr_t paddr, l4_size_t size)
{
  l4_addr_t vaddr;
  if (l4io_request_iomem(paddr, size, L4IO_MEM_CACHED, &vaddr))
    Panic("Mapping input memory from %p failed\n", (void *)paddr);

  printf("Input memory page (%lx:%zx) mapped to 0x%08lx\n", paddr, size, vaddr);

  return (void *)vaddr;
}

static int init_stuff(void)
{
  int irq = -1;
  l4io_device_handle_t devhandle = l4io_get_root_device();
  l4io_device_t dev;
  l4io_resource_handle_t reshandle;
  l4io_resource_t inputmem;
  inputmem.start = inputmem.end = 0;

  while (!l4io_iterate_devices(&devhandle, &dev, &reshandle))
    {
      l4io_resource_t res;
      if (strcmp(dev.name, "L4UXinput"))
        continue;

      while (!l4io_lookup_resource(devhandle, L4IO_RESOURCE_ANY,
                                   &reshandle, &res))
        {
          if (res.type == L4IO_RESOURCE_IRQ)
            irq = res.start;
          if (res.type == L4IO_RESOURCE_MEM)
            inputmem = res;
        }
    }


  if (!inputmem.end)
    Panic("No input memory found\n");

  input_mem = map_inputmemory(inputmem.start,
                              inputmem.end - inputmem.start + 1);

  /* only use the interrupt thread if we have a callback function
   * registered; if we're only used in polling mode we already have the
   * data and don't need async notification events
   */
  if (input_handler)
    {
      int err;

      if (irq == -1)
        Panic("Could not get IRQ number\n");

      err = request_irq(irq, irq_handler, 0, "", 0);
      if (err)
        {
          printf("Error creating IRQ thread!");
          return 1;
        }
    }

  return 0;
}

static int ux_ispending(void)
{
  struct l4input *i = input_mem + input_pos;
  return i->time;
}

static int ux_flush(void *buffer, int count)
{
  int c = 0;
  struct l4input *b = buffer;
  struct l4input *i = input_mem + input_pos;

  while (count && i->time)
    {
      i->stream_id = (unsigned long)&idev;
      *b = *i;

      i->time = 0;

      b++;

      input_pos++;
      if (input_pos == NR_INPUT_OBJS)
        input_pos = 0;
      i = input_mem + input_pos;

      count--;
      c++;
    }

  return c;
}

static struct l4input_ops ops = { ux_ispending, ux_flush, 0 };

struct l4input_ops * l4input_internal_ux_init(L4_CV void (*handler)(struct l4input *))
{
  input_handler = handler;

  if (init_stuff())
    return 0;

  void l4input_internal_register_ux(struct input_dev *dev);

  idev.id.bustype = 6;
  idev.id.vendor  = 1;
  idev.id.product = 2;
  idev.id.version = 3;

  set_bit(EV_KEY, idev.evbit);
#if 0
  for (i = 0; i < KEY_MAX; ++i)
    set_bit(i, idev.keybit);
#else
  set_bit(BTN_LEFT, idev.keybit);
  set_bit(BTN_MIDDLE, idev.keybit);
  set_bit(BTN_RIGHT, idev.keybit);
#endif

  set_bit(EV_REL, idev.evbit);
  set_bit(REL_X, idev.relbit);
  set_bit(REL_Y, idev.relbit);

  l4input_internal_register_ux(&idev);

  return &ops;
}

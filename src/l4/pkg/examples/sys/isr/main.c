/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>,
 *               Björn Döbel <doebel@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
/*
 * This example shall show how to connect to an interrupt, receive interrupt
 * events and detach again. As the interrupt source we'll use the virtual
 * key interrupt. The interrupt number of the virtual key interrupt can be
 * found in the kernel info page.
 */

#include <l4/re/c/util/cap_alloc.h>
#include <l4/re/c/namespace.h>
#include <l4/sys/utcb.h>
#include <l4/sys/irq.h>
#include <l4/sys/factory.h>
#include <l4/sys/icu.h>

#include <stdio.h>

int main(void)
{
  int irqno = 1;
  l4_cap_idx_t irqcap, icucap;
  l4_msgtag_t tag;
  int err;
  icucap = l4re_env_get_cap("icu");

  /* Get a free capability slot for the ICU capability */
  if (l4_is_invalid_cap(icucap))
    {
      printf("Did not find an ICU\n");
      return 1;
    }

  /* Get another free capaiblity slot for the corresponding IRQ object*/
  if (l4_is_invalid_cap(irqcap = l4re_util_cap_alloc()))
    return 1;
  /* Create IRQ object */
  if (l4_error(tag = l4_factory_create_irq(l4re_global_env->factory, irqcap)))
    {
      printf("Could not create IRQ object: %lx\n", l4_error(tag));
      return 1;
    }

  /*
   * Bind the recently allocated IRQ object to the IRQ number irqno
   * as provided by the ICU.
   */
  if (l4_error(l4_icu_bind(icucap, irqno, irqcap)))
    {
      printf("Binding IRQ%d to the ICU failed\n", irqno);
      return 1;
    }

  /* Bind ourselves to the IRQ */
  tag = l4_rcv_ep_bind_thread(irqcap, l4re_env()->main_thread, 0xDEAD);
  if ((err = l4_error(tag)))
    {
      printf("Error binding to IRQ %d: %d\n", irqno, err);
      return 1;
    }

  printf("Attached to key IRQ %d\nPress keys now, Shift-Q to exit\n", irqno);

  /* IRQ receive loop */
  while (1)
    {
      unsigned long label = 0;
      /* Wait for the interrupt to happen */
      tag = l4_irq_receive(irqcap, L4_IPC_NEVER);
      if ((err = l4_ipc_error(tag, l4_utcb())))
        printf("Error on IRQ receive: %d\n", err);
      else
        {
          /* Process the interrupt -- may do a 'break' */
          printf("Got IRQ with label 0x%lX\n", label);
        }
    }

  /* We're done, detach from the interrupt. */
  tag = l4_irq_detach(irqcap);
  if ((err = l4_error(tag)))
    printf("Error detach from IRQ: %d\n", err);

  return 0;
}

/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/irq/irq.h>
#include <l4/util/util.h>
#include <stdio.h>
#include <pthread.h>

enum { IRQ_NO = 17 };

static void isr_handler(void)
{
  printf("Got IRQ %d\n", IRQ_NO);
}

static void *isr_thread(void *data)
{
  l4irq_t *irq;
  (void)data;

  if (!(irq = l4irq_attach(IRQ_NO)))
    return NULL;

  while (1)
    {
      if (l4irq_wait(irq))
        continue;
      isr_handler();
    }

  return NULL;
}


int main(void)
{
  pthread_t thread;

  if (pthread_create(&thread, NULL, isr_thread, NULL))
    return 1;

  l4_sleep_forever();
  return 0;
}

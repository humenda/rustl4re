/**
 * \file
 * \brief IRQ handling routines.
 */
/*
 * (c) 2008-2011 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/irq/irq.h>
#include <l4/io/io.h>
#include <l4/sys/irq>
#include <l4/sys/icu>
#include <l4/sys/factory>
#include <l4/sys/task.h>
#include <l4/re/util/cap_alloc>
#include <l4/re/namespace>
#include <l4/re/env>

#include <pthread.h>
#include <pthread-l4.h>

#include <stdio.h>
#include <stdlib.h>

enum l4irq_type {
  IRQ_TYPE_NONE, IRQ_TYPE_L4IO, IRQ_TYPE_GIVEN,
};

struct l4irq_t {
  L4::Cap<L4::Irq>       cap;
  L4::Cap<L4::Irq_eoi>   eoi_cap;
  enum l4irq_type        type;
  unsigned               num;
  pthread_t              thread;
  void                  (*isr_func)(void *);
  void                  *isr_data;
};

static l4irq_t *release(l4irq_t *i, unsigned what)
{
  if (what >= 4) pthread_cancel(i->thread);
  if (what >= 3)
    {
      l4_irq_detach(i->cap.cap());
      if (i->type == IRQ_TYPE_L4IO)
        l4io_release_irq(i->num, i->cap.cap());
    }
  if (what >= 2) L4Re::Util::cap_alloc.free(i->cap);
  if (what >= 1) free(i);

  return NULL;
}

static l4irq_t *
alloc_and_get_irq(enum l4irq_type type, int irqnum, l4_cap_idx_t given_cap,
                  L4::Icu::Mode mode)
{
  l4irq_t *irq = (l4irq_t *)malloc(sizeof(*irq));

  if (!irq)
    return release(irq, 0);

  if (type == IRQ_TYPE_L4IO)
    {
      irq->type = IRQ_TYPE_L4IO;

      irq->cap = L4Re::Util::cap_alloc.alloc<L4::Irq>();
      if (!irq->cap.is_valid())
        return release(irq, 1);

      irq->num = irqnum;

      if (l4_error(L4Re::Env::env()->factory()->create(L4::cap_cast<L4::Irq>(irq->cap))))
	return release(irq, 2);

      L4::Cap<L4::Icu> icu(l4io_request_icu());

      int ret = l4_error(icu->set_mode(irqnum, mode));
      if (ret < 0)
        return release(irq, 2);

      ret = l4_error(icu->bind(irqnum, irq->cap));
      if (ret < 0)
	return release(irq, 2);
      if (ret == 1)
	irq->eoi_cap = icu;
      else
	irq->eoi_cap = irq->cap;
    }
  else
    {
      irq->type = IRQ_TYPE_GIVEN;
      irq->cap  = L4::Cap<L4::Irq>(given_cap);
    }

  return irq;
}

static inline long
attach_to_irq(l4irq_t *irq, L4::Cap<L4::Thread> t)
{
  return l4_error(irq->cap->bind_thread(t, (l4_umword_t)irq << 2));
}

static void *
isr_loop(void *data)
{
  l4irq_t *irq = (l4irq_t *)data;
  l4_msgtag_t res;

  if (attach_to_irq(irq, Pthread::L4::cap(pthread_self())))
    return NULL;

  while (1)
    {
      res = l4_irq_receive(irq->cap.cap(), L4_IPC_NEVER);
      if (l4_ipc_error(res, l4_utcb()))
        continue;

      irq->isr_func(irq->isr_data);
    }
}


static l4irq_t *
do_l4irq_request(enum l4irq_type type, int irqnum, l4_cap_idx_t given_cap,
                 void (*isr_handler)(void *), void *isr_data,
                 int irq_thread_prio, unsigned mode)
{
  l4irq_t *irq;

  if (!(irq = alloc_and_get_irq(type, irqnum, given_cap,
                                (L4::Icu::Mode)mode)))
    return NULL;

  irq->isr_func  = isr_handler;
  irq->isr_data  = isr_data;

  pthread_attr_t a;
  pthread_attr_init(&a);
  if (irq_thread_prio != -1)
    {
      struct sched_param sp;
      sp.sched_priority = irq_thread_prio;
      pthread_attr_setschedpolicy(&a, SCHED_L4);
      pthread_attr_setschedparam(&a, &sp);
      pthread_attr_setinheritsched(&a, PTHREAD_EXPLICIT_SCHED);
    }
  else
    pthread_attr_setinheritsched(&a, PTHREAD_INHERIT_SCHED);

  if (pthread_create(&irq->thread, &a, isr_loop, irq))
    return release(irq, 2);

  return irq;
}

l4irq_t *
l4irq_request(int irqnum, void (*isr_handler)(void *), void *isr_data,
              int irq_thread_prio, unsigned mode)
{
  return do_l4irq_request(IRQ_TYPE_L4IO, irqnum, L4_INVALID_CAP,
                          isr_handler, isr_data,
                          irq_thread_prio, mode);
}

l4irq_t *
l4irq_request_cap(l4_cap_idx_t irqcap,
                  void (*isr_handler)(void *), void *isr_data,
                  int irq_thread_prio, unsigned mode)
{
  return do_l4irq_request(IRQ_TYPE_GIVEN, -1, irqcap,
                          isr_handler, isr_data,
                          irq_thread_prio, mode);
}

long
l4irq_release(l4irq_t *irq)
{
  release(irq, ~0U);
  return 0;
}


l4irq_t *
l4irq_attach_thread(int irqnum, l4_cap_idx_t to_thread)
{
  l4irq_t *irq;

  if (!(irq = alloc_and_get_irq(IRQ_TYPE_L4IO, irqnum, L4_INVALID_CAP,
                                L4::Icu::F_none)))
    return NULL;

  if (attach_to_irq(irq, L4::Cap<L4::Thread>(to_thread)))
    return NULL;

  return irq;
}

l4irq_t *
l4irq_attach(int irqnum)
{
  return l4irq_attach_thread(irqnum, pthread_l4_cap(pthread_self()));
}

l4irq_t *
l4irq_attach_thread_cap(l4_cap_idx_t irqcap, l4_cap_idx_t to_thread)
{
  l4irq_t *irq;

  if (!(irq = alloc_and_get_irq(IRQ_TYPE_GIVEN, -1, irqcap, L4::Icu::F_none)))
    return NULL;

  if (attach_to_irq(irq, L4::Cap<L4::Thread>(to_thread)))
    return NULL;

  return irq;
}

l4irq_t *
l4irq_attach_cap(l4_cap_idx_t irqcap)
{
  return l4irq_attach_thread_cap(irqcap, pthread_l4_cap(pthread_self()));
}

long
l4irq_wait(l4irq_t *irq)
{
  l4_umword_t label;
  l4_msgtag_t res = irq->eoi_cap->unmask(irq->num, &label, L4_IPC_NEVER);
  return l4_ipc_error(res, l4_utcb());
}

long
l4irq_unmask(l4irq_t *irq)
{
  l4_msgtag_t res = l4_irq_unmask(irq->cap.cap());
  return l4_ipc_error(res, l4_utcb());
}

long
l4irq_unmask_and_wait_any(l4irq_t *unmask_irq, l4irq_t **ret_irq)
{
  l4_umword_t label;
  l4_msgtag_t res = l4_irq_wait(unmask_irq->cap.cap(), &label, L4_IPC_NEVER);
  *ret_irq = (l4irq_t *)(label >> 2);
  return l4_ipc_error(res, l4_utcb());
}

long
l4irq_wait_any(l4irq_t **irq)
{
  l4_umword_t label;
  l4_msgtag_t res = l4_ipc_wait(l4_utcb(), &label, L4_IPC_NEVER);
  *irq = (l4irq_t *)(label >> 2);
  return l4_ipc_error(res, l4_utcb());
}

long
l4irq_detach(l4irq_t *irq)
{
  release(irq, 3);
  return 0;
}

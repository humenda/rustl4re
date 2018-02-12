/*****************************************************************************/
/**
 * \file   input/lib/src/emul_wait
 * \brief  L4INPUT: Linux Wait queue emulation
 *
 * \date   2005/05/24
 * \author Frank Mehnert <fm3@os.inf.tu-dresden.de>
 *
 * I've no idea if this is really needed.
 */
/*
 * (c) 2005-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <stdio.h>
#include <l4/sys/ipc.h>
#include <l4/util/util.h>
#include <l4/util/l4_macros.h>
#include <linux/wait.h>
#include <linux/jiffies.h>

#include <pthread.h>
#include <pthread-l4.h>

l4_cap_idx_t wait_thread;

//#define dbg(format...) printf(format)
#define dbg(format...)

/** Enqueue entry into list. */
static void
enqueue_entry(wait_queue_head_t *wqh, wait_queue_entry_t *e)
{
  if (!wqh->first)
    wqh->first = e;
  else
    {
      wait_queue_entry_t *w;
      for (w=wqh->first; w->next; w=w->next)
	;
      w->next = e;
    }
}

/** Enqueue list into main list. */
static void
enqueue_head(wait_queue_head_t **wqm, wait_queue_head_t *h)
{
  if (!*wqm)
    *wqm = h;
  else
    {
      wait_queue_head_t *w = *wqm;
      for (;;)
	{
	  if (w == h)
	    return;
	  if (!w->next)
	    {
	      w->next = h;
	      return;
	    }
	  w = w->next;
	}
    }
}

/** Dequeue list from main list. */
static void
dequeue_head(wait_queue_head_t **wqm, wait_queue_head_t *h)
{
  for (; *wqm; *wqm=(*wqm)->next)
    {
      if (*wqm == h)
	{
	  *wqm = h->next;
	  return;
	}
    }
}

/** Wakeup all threads enqueued in list. */
static void
wakeup(wait_queue_head_t *h)
{
  wait_queue_entry_t *e;
  l4_cap_idx_t tid;
  l4_umword_t dummy;

  for (e=h->first; e;)
    {
      dbg("    wakeup entry %p\n", e);
      dbg("      ("l4util_idfmt", next=%p)\n", 
	  l4util_idstr(e->tid), e->next);
      tid = e->tid;
      e   = e->next;

      l4_ipc(tid, l4_utcb(), L4_SYSF_SEND, pthread_l4_cap(pthread_self()),
             l4_msgtag(0, 0, 0, 0), &dummy, L4_IPC_NEVER);
    }
  h->first = 0;
}

/** The waiter thread. */
static void *
__wait_thread(void *ignore)
{
  l4_cap_idx_t src;
  l4_umword_t dw1, dw2;
  l4_umword_t dummy;
  l4_msgtag_t r;
  l4_timeout_t to = l4_timeout(L4_IPC_TIMEOUT_NEVER,
                               l4util_micros2l4to(10000));
  int error;
  wait_queue_head_t *main_queue = 0;

  //l4thread_started(NULL);

  for (;;)
    {
      r = l4_ipc_wait(l4_utcb(), &src, main_queue ? to : L4_IPC_NEVER);
      dw1 = l4_utcb_mr()->mr[0];
      dw2 = l4_utcb_mr()->mr[1];

      error = l4_ipc_error(r, l4_utcb());

      if (error == 0)
	{
	  /* got request */
	  if (dw1 != 0)
	    {
	      /* go sleep, append at wait queue */
	      wait_queue_head_t  *h = (wait_queue_head_t*)dw1;
	      wait_queue_entry_t *e = (wait_queue_entry_t*)dw2;

	      dbg("enqueue "l4util_idfmt" into queue %p entry %p\n", l4util_idstr(src), (void*)dw1, (void*)dw2);

	      e->tid  = src;
	      e->next = 0;

	      /* append entry to wait queue */
	      enqueue_entry(h, e);
	      /* append wait queue to main queue if necessary */
	      enqueue_head(&main_queue, h);

	      dbg("  queue now ");
	      for (e=h->first; e; e=e->next)
		dbg("%p ", e);
	      dbg("\n");
	    }
	  else
	    {
	      /* wakeup */
	      wait_queue_head_t *h = ((wait_queue_head_t*)dw2);
	      dbg("wakeup queue %p\n", (void*)dw2);
	      /* wakeup waiting threads of wait queue */
	      wakeup(h);
	      /* dequeue wait queue from main queue */
	      dequeue_head(&main_queue, h);
	      dbg("  main=%p\n", (void*)main_queue);
              l4_ipc(src, l4_utcb(), L4_SYSF_SEND,
                     pthread_l4_cap(pthread_self()),
                     l4_msgtag(0, 0, 0, 0), &dummy, L4_IPC_NEVER);
	    }
	}
      else if (error == L4_IPC_RETIMEOUT)
	{
	  /* timeout, wakeup all queues */
	  wait_queue_head_t *h;
	  dbg("wakup all queues\n");
	  for (h=main_queue; h; h=h->next)
	    {
	      dbg("  wakeup queue %p\n", h);
	      wakeup(h);
	    }
	  main_queue = 0;
	}
      else
	{
	  /* ??? */
	}
    }
  return NULL;
}

/** Called if a specific wait queue should be woken up. */
void
wake_up(wait_queue_head_t *wq)
{
  l4_umword_t dummy;

  l4_utcb_mr()->mr[0] = 0;
  l4_utcb_mr()->mr[1] = (l4_umword_t)(wq);
  l4_ipc(wait_thread, l4_utcb(), L4_SYSF_CALL,
         pthread_l4_cap(pthread_self()),
         l4_msgtag(0, 2, 0, 0),
         &dummy, L4_IPC_NEVER);
}

/** Initialization. */
void
l4input_internal_wait_init(void)
{
  pthread_t t;
  pthread_attr_t a;
  struct sched_param sp;

  pthread_attr_init(&a);
  sp.sched_priority = 255;
  pthread_attr_setschedpolicy(&a, SCHED_L4);
  pthread_attr_setschedparam(&a, &sp);
  pthread_attr_setinheritsched(&a, PTHREAD_EXPLICIT_SCHED);

  if (pthread_create(&t, &a, __wait_thread, NULL))
    return;

  wait_thread = pthread_l4_cap(t);

  pthread_attr_destroy(&a);
}

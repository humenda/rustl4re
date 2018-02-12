/**
 * \file
 * \brief Low-level example of communication.
 *
 * This example shows how two threads can exchange data using the L4 IPC
 * mechanism.
 */
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
#include <l4/sys/ipc.h>
#include <l4/sys/thread.h>
#include <l4/sys/factory.h>
#include <l4/sys/utcb.h>
#include <l4/re/env.h>
#include <l4/re/c/util/cap_alloc.h>
#include <l4/util/thread.h>

#include <stdio.h>
#include <string.h>

static unsigned char stack2[8 << 10];
static l4_cap_idx_t thread1_cap, thread2_cap;

static void thread1(void)
{
  l4_msg_regs_t *mr = l4_utcb_mr();
  l4_msgtag_t tag;
  int i, j;

  printf("Thread1 up (%p)\n", l4_utcb());

  for (i = 0; i < 10; i++)
    {
      for (j = 0; j < L4_UTCB_GENERIC_DATA_SIZE; j++)
        mr->mr[j] = 'A' + (i + j) % ('~' - 'A' + 1);
      tag = l4_msgtag(0, L4_UTCB_GENERIC_DATA_SIZE, 0, 0);
      if (l4_msgtag_has_error(l4_ipc_send(thread2_cap, l4_utcb(), tag, L4_IPC_NEVER)))
	printf("IPC-send error\n");
    }
}

L4UTIL_THREAD_STATIC_FUNC(thread2)
{
  l4_msgtag_t tag;
  l4_msg_regs_t mr;
  unsigned i;

  printf("Thread2 up (%p)\n", l4_utcb());

  while (1)
    {
      if (l4_msgtag_has_error(tag = l4_ipc_receive(thread1_cap, l4_utcb(), L4_IPC_NEVER)))
        printf("IPC receive error\n");
      memcpy(&mr, l4_utcb_mr(), sizeof(mr));
      printf("Thread2 receive (%d): ", l4_msgtag_words(tag));
      for (i = 0; i < l4_msgtag_words(tag); i++)
        printf("%c", (char)mr.mr[i]);
      printf("\n");
    }

  __builtin_trap();
}

int main(void)
{
  l4_msgtag_t tag;

  thread1_cap = l4re_env()->main_thread;
  thread2_cap = l4re_util_cap_alloc();

  if (l4_is_invalid_cap(thread2_cap))
    return 1;

  tag = l4_factory_create_thread(l4re_env()->factory, thread2_cap);
  if (l4_error(tag))
    return 1;

  l4_thread_control_start();
  l4_thread_control_pager(l4re_env()->rm);
  l4_thread_control_exc_handler(l4re_env()->rm);
  l4_thread_control_bind((l4_utcb_t *)l4re_env()->first_free_utcb,
                          L4RE_THIS_TASK_CAP);
  tag = l4_thread_control_commit(thread2_cap);
  if (l4_error(tag))
    return 2;

  tag = l4_thread_ex_regs(thread2_cap,
                          (l4_umword_t)thread2,
                          (l4_umword_t)(stack2 + sizeof(stack2)), 0);
  if (l4_error(tag))
    return 3;

  l4_sched_param_t sp = l4_sched_param(1, 0);
  tag = l4_scheduler_run_thread(l4re_env()->scheduler, thread2_cap, &sp);
  if (l4_error(tag))
    return 4;

  thread1();

  return 0;
}

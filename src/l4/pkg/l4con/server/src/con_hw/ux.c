/*!
 * \file	ux.c
 * \brief	Active screen update pushing
 *
 * \date	11/2003
 * \author	Adam Lackorzynski <adam@os.inf.tu-dresden.de> */
/*
 * (c) 2003-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <stdio.h>
#include <string.h>
#include <l4/sys/err.h>
#include <l4/sys/types.h>
#include <l4/sys/vhw.h>
#include <l4/sys/thread.h>
#include <l4/sys/kdebug.h>
#include <l4/sys/ipc.h>
#include <l4/util/l4_macros.h>
#include <l4/util/util.h>
#include <l4/util/kip.h>
#include <l4/io/io.h>
#include <l4/re/env.h>
#include <l4/lxfuxlibc/lxfuxlc.h>

#include <pthread.h>
#include <pthread-l4.h>

#include "init.h"
#include "iomem.h"
#include "ux.h"

static lx_pid_t ux_con_pid;

static volatile int waiting;
static l4_cap_idx_t updater_id;

static void *updater_thread(void *data)
{
  l4_umword_t l;
  (void)data;

  l4_thread_control_start();
  l4_thread_control_ux_host_syscall(1);
  l4_thread_control_commit(pthread_l4_cap(pthread_self()));

  while (1)
    {
      if (l4_ipc_error(l4_ipc_wait(l4_utcb(), &l, L4_IPC_NEVER), l4_utcb()))
        printf("updater_thread: l4_ipc_wait failed\n");
      l4_sleep(30);
      lx_kill(ux_con_pid, LX_SIGUSR1);
      waiting = 0;
    }
  return NULL;
}

static void
uxScreenUpdate(int x, int y, int w, int h)
{
  (void)x; (void)y; (void)w; (void)h;
  if (!waiting)
    {
      lx_kill(ux_con_pid, LX_SIGUSR1);
      waiting = 1;
      while (l4_ipc_error(l4_ipc_send(updater_id, l4_utcb(),
                                      l4_msgtag(0, 0, 0, 0),
                                      L4_IPC_BOTH_TIMEOUT_0), l4_utcb()))
        {
          l4_sleep(1);
        }
    }
}

#if 0
static void
uxRectFill(struct l4con_vc *vc,
           int sx, int sy, int width, int height, unsigned color)
{
  //printf("%s\n", __func__);
}

static void
uxRectCopy(struct l4con_vc *vc, int sx, int sy,
           int width, int height, int dx, int dy)
{
  //printf("%s\n", __func__);
}
#endif

int
ux_probe(con_accel_t *accel)
{
  struct l4_vhw_descriptor *vhw;
  struct l4_vhw_entry *vhwe;

  if (!l4util_kip_kernel_is_ux(l4re_kip()))
    return -L4_ENODEV;

  if (l4io_lookup_device("L4UXfb", NULL, 0, 0))
    {
      printf("WARNING: Running under Fiasco-UX and no 'L4UXfb' device found, can only use polling mode!\n");
      return -L4_ENODEV;
    }

  printf("Found Fiasco-UX Framebuffer\n");

  if (!(vhw = l4_vhw_get(l4re_kip())))
    return -L4_ENODEV;

  if (!(vhwe = l4_vhw_get_entry_type(vhw, L4_TYPE_VHW_FRAMEBUFFER)))
    return -L4_ENODEV;

  ux_con_pid = vhwe->provider_pid;
  accel->drty = uxScreenUpdate;
  accel->caps = ACCEL_POST_DIRTY;

  printf("Found VHW descriptor, provider is %d\n", ux_con_pid);

  l4_thread_control_start();
  l4_thread_control_ux_host_syscall(1);
  l4_thread_control_commit(l4re_env()->main_thread);

  /* The update thread needs to run with the same priority than as the vc
   * threads (otherwise the screen won't be updated) */
  {
    pthread_t update_tid;
    pthread_attr_t a;
    struct sched_param sp;

    pthread_attr_init(&a);
    sp.sched_priority = 0x70;
    pthread_attr_setschedpolicy(&a, SCHED_L4);
    pthread_attr_setschedparam(&a, &sp);
    pthread_attr_setinheritsched(&a, PTHREAD_EXPLICIT_SCHED);

    if (pthread_create(&update_tid, &a, updater_thread, NULL))
       return -L4_ENOMEM;
    updater_id = pthread_l4_cap(update_tid);

#if 0
    update_tid = l4thread_create_long(L4THREAD_INVALID_ID,
                                      updater_thread, ".scr-upd",
                                      L4THREAD_INVALID_SP, L4THREAD_DEFAULT_SIZE,
                                      0xff, NULL,
                                      L4THREAD_CREATE_ASYNC);
    if (update_tid < 0)
      {
        printf("Could not create updater thread.\n");
        return -L4_ENOTHREAD;
      }

    updater_id = l4thread_l4_id(update_tid);
#endif
  }

#if 0
  if (hw_vid_mem_addr != vhwe->mem_start
      || hw_vid_mem_size != vhwe->mem_size)
    printf("!!! Memory area mismatch "l4_addr_fmt"(%lx) vs. "l4_addr_fmt
           "(%lx) ... continuing\n",
           hw_vid_mem_addr, hw_vid_mem_size, vhwe->mem_start, vhwe->mem_size);
#endif

  //map_io_mem(hw_vid_mem_addr, hw_vid_mem_size, 0, "UX video",
   //          (l4_addr_t *)&hw_map_vid_mem_addr);

  // already mapped by vesa-ds + rm-attach
  hw_map_vid_mem_addr = hw_vid_mem_addr;

  return 0;
}

/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2020 Kernkonzept GmbH.
 * Author(s): Alexander Warg <alexander.warg@kernkonzept.com>
 */

#include <l4/re/util/object_registry>
#include <l4/sys/cxx/ipc_server_loop>

#include <pthread.h>
#include <pthread-l4.h>
#include <errno.h>

#include "irq_server.h"
#include "debug.h"

namespace {

typedef L4Re::Util::Registry_server<> Irq_server;
static Irq_server *irq_server;

static void *_server_loop_func(void *_svr)
{
  Irq_server *svr = static_cast<Irq_server *>(_svr);
  svr->loop();
  return 0;
}

}

L4Re::Util::Object_registry *irq_queue()
{
  if (irq_server)
    return irq_server->registry();

  pthread_t irq_server_thread;
  int e = pthread_create(&irq_server_thread, NULL, NULL, NULL);
  if (e != 0)
    {
      d_printf(DBG_ERR, "fatal: could not create IRQ handler thread: %d\n",
               -errno);
      return 0;
    }

  irq_server = new Irq_server(Pthread::L4::utcb(irq_server_thread),
                              Pthread::L4::cap(irq_server_thread),
                              L4Re::Env::env()->factory());

  e = Pthread::L4::start(irq_server_thread, _server_loop_func, irq_server);
  if (e < 0)
    {
      delete irq_server;
      irq_server = 0;
      d_printf(DBG_ERR, "fatal: could not start IRQ handler thread: %d\n",
               e);
      return 0;
    }
  d_printf(DBG_DEBUG, "created IRQ handler thread\n");
  return irq_server->registry();
}




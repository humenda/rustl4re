/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/re/env>
#include <l4/re/namespace>
#include <l4/re/util/cap_alloc>
#include <l4/re/util/object_registry>
#include <l4/re/util/br_manager>
#include <l4/re/error_helper>

#include <l4/sys/cxx/ipc_server_loop>
#include <l4/cxx/ipc_timeout_queue>

#include <pthread.h>
#include <pthread-l4.h>
#include <errno.h>

#include "debug.h"
#include "server.h"

class Loop_hooks :
  public L4::Ipc_svr::Timeout_queue_hooks<Loop_hooks, L4Re::Util::Br_manager>,
  public L4::Ipc_svr::Ignore_errors
{
public:
  static l4_kernel_clock_t now()
  { return l4_kip_clock(l4re_kip()); }
};

static L4Re::Util::Registry_server<Loop_hooks> server;

L4Re::Util::Object_registry *registry = server.registry();

int server_loop()
{
  server.loop();
  return 0;
}

typedef L4Re::Util::Registry_server<Loop_hooks> Irq_server;
static Irq_server *irq_server;

static void *_server_loop_func(void *_svr)
{
  Irq_server *svr = static_cast<Irq_server *>(_svr);
  svr->loop();
  return 0;
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



/*
 * Example for mapping a capability from a client to a server - client part.
 *
 * (c) 2014 Steffen Liebergeld <steffen.liebergeld@kernkonzept.com>
 *
 * This file is licensed under the terms of the GNU General Public License 2.
 * See file COPYING-GPL-2 for details.
 */
#include <l4/sys/factory>
#include <l4/sys/irq>
#include <l4/sys/ipc_gate>
#include <l4/cxx/ipc_stream>
#include <l4/re/env>
#include <l4/re/error_helper>
#include <l4/re/util/cap_alloc>

#include <cstdio>

#include "shared.h"

enum { Nr_of_triggers = 3 };

static int run()
{
  using L4Re::chksys;
  using L4Re::chkcap;

  printf("Hello from ex_map_irq_client.\n");

  // allocate cap for IRQ
  L4::Cap<L4::Irq> irq = chkcap(L4Re::Util::cap_alloc.alloc<L4::Irq>(),
                                "could not find a free cap slot");
  // create IRQ kernel object
  chksys(L4Re::Env::env()->factory()->create(irq),
         "create a new IRQ kernel object");

  // look out for server
  L4::Cap<Irq_source> server;
  server = chkcap(L4Re::Env::env()->get_cap<Irq_source>("ex_map_irq"),
                  "get 'ex_map_irq' capability");

  // map irq to server
  printf("Mapping IRQ cap to server.\n");
  chksys(server->map_irq(irq), "map irq");

  // bind to IRQ and wait for the server to trigger it
  chksys(irq->bind_thread(L4Re::Env::env()->main_thread(), 0),
         "bind to IRQ");

  // tell the server to start triggering us and how many times it should
  // trigger the IRQ
  chksys(server->start(Nr_of_triggers), "starting triggers");


  for (int i = 0; i < Nr_of_triggers; ++i)
    {
      chksys(irq->receive(), "receiving IRQ");
      printf("Received IRQ.\n");
    }

  printf("ex_map_irq_client finished.\n");
  return 0;
}

int main()
{
  try
    {
      return run();
    }
  catch (L4::Runtime_error &e)
    {
      printf("Runtime error: %s.\n", e.str());
    }
  catch (...)
    {
      printf("Uncaught error.\n");
    }
  return 1;
}


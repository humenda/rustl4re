/*
 * Example for mapping a capability from a client to a server - server part.
 *
 * (c) 2014 Steffen Liebergeld <steffen.liebergeld@kernkonzept.com>
 *
 * This file is licensed under the terms of the GNU General Public License 2.
 * See file COPYING-GPL-2 for details.
 */
#include <l4/re/error_helper>
#include <l4/sys/cxx/ipc_epiface>
#include <l4/sys/irq>
#include <l4/re/env>
#include <l4/sys/factory>
#include <l4/re/util/object_registry>
#include <l4/re/util/br_manager>

#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <stdlib.h>

#include "shared.h"

// Those are error helpers that simplify our code by hiding error checking.
// Errors flagged through C++ exceptions.
using L4Re::chksys;
using L4Re::chkcap;

// This class reacts on notifications from Server
class Trigger : public L4::Irqep_t<Trigger>
{
public:
  void handle_irq()
  {
    for (unsigned i = 0; i < _nrs; ++i)
      {
        printf("Triggering IRQ.\n");
        _irq->trigger();
        sleep(1);
      }
    printf("ex_map_irq_server finished.\n");
    exit(0);
  }

  void num_triggers(unsigned num);
  void irq(L4::Cap<L4::Irq> irq);

  void trigger() { _irq->trigger(); }

private:
  L4::Cap<L4::Irq> _irq;
  unsigned _nrs;
};

void Trigger::num_triggers(unsigned num)
{
  _nrs = num;
}

void Trigger::irq(L4::Cap<L4::Irq> irq)
{
  _irq = irq;
}

// This class handles the reception of the IRQ cap from the client.
class Server : public L4::Epiface_t<Server, Irq_source>
{
public:
  Server(L4::Cap<L4::Irq> irq, Trigger *trigger)
  : _trigger_notification(irq), _trigger(trigger)
  {}

  int op_map_irq(Irq_source::Rights, L4::Ipc::Snd_fpage const &irq);
  int op_start(Irq_source::Rights, unsigned nrs);

private:
  void do_client_trigger(unsigned);

  L4::Cap<L4::Irq> _trigger_notification;
  Trigger *_trigger;
};

int
Server::op_map_irq(Irq_source::Rights, L4::Ipc::Snd_fpage const &irq)
{
  if (!irq.cap_received())
    return -L4_EINVAL;

  // We use rcv_cap for receiving only.
  // After receiving the cap, we tell the kernel to associate
  // the kernel object that rcv_cap points to with irq.

  // allocate new cap
  _trigger->irq(L4Re::chkcap(server_iface()->rcv_cap<L4::Irq>(0),
                             "reading receive capability 0"));

  L4Re::chksys(server_iface()->realloc_rcv_cap(0),
               "allocating new receive capability");

  printf("Received IRQ from client.\n");
  return L4_EOK;
}

int
Server::op_start(Irq_source::Rights, unsigned nrs)
{
  _trigger->num_triggers(nrs);
  _trigger_notification->trigger();
  return L4_EOK;
}

static L4Re::Util::Registry_server<L4Re::Util::Br_manager_hooks> server;

static int run()
{
  printf("Hello from ex_map_irq_server.\n");

  Trigger trigger;
  L4::Cap<L4::Irq> notification_irq;

  notification_irq = chkcap(server.registry()->register_irq_obj(&trigger),
                            "could not register notification trigger");

  Server map_irq_srv(notification_irq, &trigger);

  L4Re::chkcap(server.registry()->register_obj(&map_irq_srv, "ex_map_irq"),
               "could not register service side");

  server.loop();
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

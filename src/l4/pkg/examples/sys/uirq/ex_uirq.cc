/*
 * (c) 2014 Steffen Liebergeld <steffen.liebergeld@kernkonzept.com>
 *
 * This file is licensed under the terms of the GNU General Public License 2.
 * See file COPYING-GPL-2 for details.
 */

/*
 * This example shows how to create a user IRQ and how to receive and trigger
 * them. Triggering and receiving is done in different threads, created with
 * C++11's std::thread.
 */
#include <l4/re/env>
#include <l4/re/error_helper>
#include <l4/re/util/cap_alloc>
#include <l4/sys/irq>
#include <l4/sys/factory>

#include <pthread-l4.h>
#include <thread>

#include <cstdio>
#include <unistd.h>

int main()
{
  try
    {
      printf("IRQ triggering example.\n");

      L4::Cap<L4::Irq> irq;

      // Allocate a capability to use for the IRQ object
      irq = L4Re::chkcap(L4Re::Util::cap_alloc.alloc<L4::Irq>());

      // Create the IRQ object with the capability, using our default
      // factory
      L4Re::chksys(L4Re::Env::env()->factory()->create(irq),
                   "Failed to create IRQ.");

      // bind to ourselves to the IRQ to receive its triggers
      L4Re::chksys(irq->bind_thread(Pthread::L4::cap(pthread_self()), 0),
                   "Could not bind to IRQ.");

      // Create a thread and also tell it about our IRQ capability
      std::thread thread = std::thread([irq](){ irq->trigger(); });

      L4Re::chksys(irq->receive(), "Receive failed.");
      printf("Received irq!\n");

      // 'Wait' for thread to finish
      thread.join();

      printf("Example finished.\n");
      return 0;
    }
  catch (L4::Runtime_error &e)
    {
      fprintf(stderr, "Runtime error: %s.\n", e.str());
    }
  catch (...)
    {
      fprintf(stderr, "Unknown exception.\n");
    }

  return 1;
}

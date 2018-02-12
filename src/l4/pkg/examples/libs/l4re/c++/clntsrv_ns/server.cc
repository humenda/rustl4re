/*
 * Client-server example using namespaces. Server component.
 * 
 * (c) 2008-2013 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>
 *               Bjoern Doebel  <doebel@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <stdio.h>
#include <l4/re/env>
#include <l4/re/namespace>
#include <l4/re/util/cap_alloc>
#include <l4/re/util/object_registry>
#include <l4/sys/cxx/ipc_epiface>
#include <l4/sys/cxx/ipc_server_loop>

#include "shared.h"

static L4Re::Util::Registry_server<> server;

class Calculation_server : public L4::Epiface_t<Calculation_server, Calc>
{
public:
  int op_sub(Calc::Rights, l4_uint32_t a, l4_uint32_t b, l4_uint32_t &res)
  {
    res = a - b;
    return 0;
  }

  int op_neg(Calc::Rights, l4_uint32_t a, l4_uint32_t &res)
  {
    res = -a;
    return 0;
  }
};


int
main()
{
  static Calculation_server calc;
  L4::Cap<L4Re::Namespace> ns;

  /*
   * In this example we register the object _without_ a name
   * at the registry server. This will create a new IPC gate,
   * attach our server object, and bind the current thread to it.
   */
  if (!server.registry()->register_obj(&calc).is_valid())
    {
      printf("Could not register server object\n");
      return 1;
    }

  /*
   * We get an empty namespace through our initial
   * set of capabilities. (See ns.cfg)
   */
  ns = L4Re::Env::env()->get_cap<L4Re::Namespace>("namespace");
  if (!ns.is_valid())
    {
      printf("Could not find namespace\n");
      return 1;
    }

  /*
   * Register object in the namespace.
   *
   * NOTE: This is *not* the same as registering the object
   *       in our local registry. The registry tracks local
   *       server objects. The namespace tracks capabilities
   *       across tasks.
   */
  long r = ns->register_obj("the_object", calc.obj_cap());
  if (r < 0)
    {
      printf("Error registering object in namespace: %ld\n", r);
      return 1;
    }

  printf("Welcome to the calculation server!\n"
         "I can do substractions and negations.\n");

  // Wait for client requests
  server.loop();

  return 0;
}

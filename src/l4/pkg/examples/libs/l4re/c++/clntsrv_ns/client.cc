/*
 * Client-server example using namespaces. Client component.
 * 
 * (c) 2008-2013 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>,
 *               Bjoern Doebel <doebel@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/sys/err.h>
#include <l4/sys/types.h>
#include <l4/re/env>
#include <l4/re/namespace>
#include <l4/re/util/cap_alloc>

#include <stdio.h>

#include "shared.h"

int
main()
{
  /*
   * Get the namespace cap which comes from
   * our initial set of capabilities. (See ns.cfg)
   */
  L4::Cap<L4Re::Namespace> ns = L4Re::Env::env()->get_cap<L4Re::Namespace>("namespace");
  if (!ns.is_valid())
    {
      printf("Could not find namespace\n");
      return 1;
    }

  /*
   * We will query the (shared) namespace for an object. This object needs
   * to go into a local capability slot. Hence, we need to reserve one here.
   */
  L4::Cap<Calc> server = L4Re::Util::cap_alloc.alloc<Calc>();

  if (!server.is_valid())
    {
      printf("Could not get server capability!\n");
      return 1;
    }

  /*
   * Now query the object name. This blocks until the object appears.
   */
  long r = ns->query("the_object", server, L4Re::Namespace::To_forever);
  if (r < 0)
    {
      printf("Error querying object: %ld\n", r);
      return 1;
    }

  l4_uint32_t val1 = 8;
  l4_uint32_t val2 = 5;

  printf("Asking for %d - %d\n", val1, val2);

  if (server->sub(val1, val2, &val1))
    {
      printf("Error talking to server\n");
      return 1;
    }
  printf("Result of substract call: %d\n", val1);
  printf("Asking for -%d\n", val1);
  if (server->neg(val1, &val1))
    {
      printf("Error talking to server\n");
      return 1;
    }
  printf("Result of negate call: %d\n", val1);

  return 0;
}

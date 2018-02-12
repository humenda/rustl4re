/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/sys/ipc.h>
#include <stdio.h>
#include <l4/re/l4aux.h>
#include <l4/re/error_helper>
#include <l4/re/util/br_manager>
#include <l4/sys/scheduler>
#include <l4/sys/thread>
#include <l4/cxx/iostream>

#include <typeinfo>

#include "debug.h"
#include "region.h"
#include "global.h"
#include "log.h"
#include "obj_reg.h"
#include "alloc.h"

using L4Re::chksys;

static Dbg info(Dbg::Info);
static Dbg boot_info(Dbg::Boot);


l4re_aux_t* l4re_aux;
template< typename Reg >
class My_dispatcher
{
private:
  Reg r;

public:
  l4_msgtag_t dispatch(l4_msgtag_t tag, l4_umword_t obj, l4_utcb_t *utcb)
  {
    typename Reg::Value *o = 0;

    Dbg dbg(Dbg::Server);

    dbg.printf("tag=%lx (proto=%ld) obj=%lx", tag.raw,
               tag.label(), obj);

    if (tag.is_exception())
      {
	dbg.cprintf("\n");
	Dbg(Dbg::Warn).printf("unhandled exception...\n");
	return l4_msgtag(-L4_ENOREPLY, 0, 0, 0);
      }
    else
      {
	o = r.find(obj & ~3UL);

	if (!o)
	  {
	    dbg.cprintf(": invalid object\n");
	    return l4_msgtag(-L4_ENOENT, 0, 0, 0);
	  }

	dbg.cprintf(": object is a %s\n", typeid(*o).name());
	l4_msgtag_t res = o->dispatch(tag, obj, utcb);
	dbg.printf("reply = %ld\n", res.label());
	return res;
      }

    Dbg(Dbg::Warn).printf("Invalid message (tag.label=%ld)\n", tag.label());
    return l4_msgtag(-L4_ENOSYS, 0, 0, 0);
  }

};


static L4::Server<L4Re::Util::Br_manager_hooks> server(l4_utcb());

namespace Gate_alloc {
  L4Re::Util::Object_registry registry(&server);
}


int
main(int argc, char **argv)
{
  try {
  Dbg::set_level(0); //Dbg::Info | Dbg::Boot);

  info.printf("Hello from loader\n");
  boot_info.printf("cmdline: ");
  for (int i = 0; i < argc; ++i)
    boot_info.cprintf("'%s'", argv[i]);
  boot_info.cprintf("\n");

  l4_umword_t *auxp = (l4_umword_t*)&argv[argc] + 1;
  while (*auxp)
    ++auxp;
  ++auxp;

  l4re_aux = 0;

  while (*auxp)
    {
      info.printf("aux: %lx\n", *auxp);
      if (*auxp == 0xf0)
	l4re_aux = (l4re_aux_t*)auxp[1];
      auxp += 2;
    }

  Region_map::global_init();


  L4Re::chkcap(Gate_alloc::registry.register_obj(Allocator::root_allocator(), "loader"));

  server.loop<L4::Runtime_error>(My_dispatcher<L4::Basic_registry>());
  }
  catch (L4::Runtime_error const &e)
    {
      L4::cout << "Got exception\n";
      L4::cout << e << "\n";
    }
  return 1;
}

/*
 * Copyright (C) 2009-2019 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *            Marcus Hähnel <marcus.haehnel@kernkonzept.com>
 *
 * This file is distributed under the terms of the GNU General Public
 * License, version 2. Please see the COPYING-GPL-2 file for details.
 */

#include "object_registry_gc"
#include <l4/re/error_helper>

class Del_handler : public L4::Irqep_t<Del_handler>
{
private:
  Object_registry_gc *_r;

public:
  explicit Del_handler(Object_registry_gc *r) : _r(r) {}

  void handle_irq()
  { _r->gc_step(); }
};

Object_registry_gc::Object_registry_gc(L4::Ipc_svr::Server_iface *sif)
: Object_registry(sif)
{
  L4::Cap<L4::Irq> _del_irq
    = L4Re::chkcap(register_irq_obj(new Del_handler(this)));
  _server->register_del_irq(_del_irq);
}

L4::Cap<void>
Object_registry_gc::register_obj_with_gc(Server_object *o)
{
  L4::Cap<L4::Kobject> c
    = L4::cap_cast<L4::Kobject>(Object_registry::register_obj(o));

  if (!c)
    return c;

  _life.add(o);
  c->dec_refcnt(1);

  return c;
}

void
Object_registry_gc::gc_step()
{
  for (Obj_iter n = _life.begin(); n != _life.end(); )
    {
      if (!n->obj_cap() || !n->obj_cap().validate().label())
        {
          unregister_obj(*n);
          Server_object *o = *n;
          delete o;
        }
    }
}

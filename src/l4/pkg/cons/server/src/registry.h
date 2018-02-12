/*
 * (c) 2012-2013 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include "server.h"
#include <l4/re/util/object_registry>

class Registry : public L4Re::Util::Object_registry
{
private:
  Registry(Registry const &);
  void operator = (Registry const &);

  typedef cxx::H_list<Server_object> Obj_list;
  typedef Obj_list::Iterator Obj_iter;

  Obj_list _life;
  Obj_list _sweep;

public:
  Registry(L4::Ipc_svr::Server_iface *sif);

  void gc_sweep();
  void gc_step();

  L4::Cap<void> register_obj(Server_object *o, char const *service);
  L4::Cap<void> register_obj(Server_object *o);
};

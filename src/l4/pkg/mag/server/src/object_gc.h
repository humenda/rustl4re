// vi:ft=cpp
/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#pragma once

#include <l4/mag/server/object>

namespace Mag_server {

class Object_gc
{
private:
  Object_gc(Object_gc const &);
  void operator = (Object_gc const &);

protected:
  typedef cxx::H_list<Object> Obj_list;
  typedef Obj_list::Iterator Obj_iter;

  Obj_list _life;
  Obj_list _sweep;

public:
  Object_gc() {}

  void gc_sweep();
  void gc_step();
  void add_obj(Object *o) { o->enqueue(&_life); }

  virtual void gc_obj(Object *o) = 0;
};

}

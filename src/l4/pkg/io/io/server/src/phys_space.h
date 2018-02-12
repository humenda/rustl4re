/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include <l4/cxx/avl_set>
#include <l4/sys/l4int.h>

class Phys_space
{
public:
  class Phys_region
  {
  public:
    typedef l4_addr_t Addr;

    Phys_region() : _s(0), _e(0) {}
    Phys_region(Addr s, Addr e) : _s(s), _e(e) {}

    Addr start() const { return _s; }
    Addr end() const { return _e; }
    Addr size() const { return _e - _s + 1; }

    bool operator < (Phys_region const &o) const throw()
    { return _e < o._s; }
    bool contains(Phys_region const &o) const throw()
    { return o._s >= _s && o._e <= _e; }
    bool operator == (Phys_region const &o) const throw()
    { return o._s == _s && o._e == _e; }

    bool valid() const { return _e > _s; }

    void set_start(Addr s) { _s = s; }
    void set_end(Addr e) { _e = e; }

  private:
    Addr _s, _e;
  };

  Phys_space();

  bool reserve(Phys_region const &r);
  bool alloc(Phys_region const &r);
  Phys_region alloc(Phys_region::Addr sz, Phys_region::Addr align);

  void dump();

  static Phys_space space;

private:
  typedef cxx::Avl_set<Phys_region> Set;

  Set _set;

  bool alloc_from(Set::Iterator const &o, Phys_region const &r);

};

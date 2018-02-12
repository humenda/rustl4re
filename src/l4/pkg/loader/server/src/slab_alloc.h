/*
 * (c) 2008-2009 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include <l4/cxx/slab_alloc>

class Single_page_alloc_base
{
protected:
  void *_alloc();
  void _free(void *p);
};

template<typename A>
class Single_page_alloc : public Single_page_alloc_base
{
public:
  enum { can_free = true };
  A *alloc() { return reinterpret_cast<A*>(_alloc()); }
  void free(A *o) { _free(o); }
};

template< typename Type >
class Slab_alloc 
: public cxx::Slab_static<Type, L4_PAGESIZE, 2, Single_page_alloc>
{};



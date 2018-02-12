/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>,
 *               Frank Mehnert <fm3@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#ifndef REGION_H
#define REGION_H

#include <l4/sys/compiler.h>
#include <l4/sys/l4int.h>

#include "types.h"

/** Region in memory. */
class Region
{
public:
  enum Type { No_mem, Kernel, Sigma0, Boot, Root, Arch, Ram, Info };

  /** Basic noop constructor, to be able to have array without ini code */
  Region() {}

  /** Create an invalid region. */
  Region(Type) : _begin(0), _end(0) {}

  /** Create a 1byte region at begin, basically for lookups */
  Region(unsigned long long begin)
  : _begin(begin), _end(begin), _name(0), _t(No_mem), _s(0)
  {}

  /** Create a 1byte region for address \a ptr.
   * @param ptr the start address for the 1byte region.
   */
  Region(void const *ptr)
  : _begin((l4_addr_t)ptr), _end((l4_addr_t)ptr), _name(0), _t(No_mem), _s(0)
  {}

  /** Create a fully fledged region.
   * @param begin The start address.
   * @param end The address of the last byte in the region.
   * @param name The name for the region (usually the binary name).
   * @param t The type of the region.
   * @param sub The subtype of the region.
   */
  Region(unsigned long long begin, unsigned long long end,
         char const *name = 0, Type t = No_mem, short sub = 0)
  : _begin(begin), _end(end), _name(name), _t(t), _s(sub)
  {}

  /**
   * Create a region ...
   * @param other a region to copy the name, the type, and the sub_type from
   * @param begin the start address of the new region
   * @param end the end address (inclusive) of the new region
   */
  Region(Region const &other, unsigned long long begin, unsigned long long end)
  : _begin(begin), _end(end), _name(other._name), _t(other._t), _s(other._s)
  {}

  /**
   * Create a region...
   * @param begin The start address.
   * @param end The address of the first byte after the region.
   * @param name The name for the region (usually the binary name).
   * @param t The type of the region.
   * @param sub The subtype of the region.
   */
  static Region n(unsigned long long begin,
                  unsigned long long end, char const *name = 0,
                  Type t = No_mem, short sub = 0)
  { return Region(begin, end - 1, name, t, sub); }

  /**
   * Create a region (using start and end pointers)
   * @param begin start address
   * @param end The address of the first byte after the region.
   * @param name The name of the region
   * @param t the type of the region
   * @param sub the subtype of the region
   */
  static Region n(void const *begin,
                  void const *end, char const *name = 0,
                  Type t = No_mem, short sub = 0)
  { return Region((l4_addr_t)begin, (l4_addr_t)end - 1, name, t, sub); }

  /**
   * Create a region from start and size.
   * @param begin the start address of the region
   * @param size the size of the region in bytes
   * @param name the name of the region
   * @param t the type of the region
   * @param sub the subtype of the region
   */
  static Region start_size(unsigned long long begin, unsigned long size,
                           char const *name = 0, Type t = No_mem,
                           short sub = 0)
  { return Region(begin, begin + size - 1, name, t, sub); }

  /**
   * Create a region for the given object.
   * @param begin a pointer to the object that shall be covered
   *              (the size of the region will be sizeof(T))
   * @param name the name of the region
   * @param t the type of theregion
   * @param sub the subtype of the region
   */
  template< typename T >
  static Region from_ptr(T const *begin, char const *name = 0,
                         Type t = No_mem, short sub = 0)
  { return Region::start_size((l4_addr_t)begin, sizeof(T), name, t, sub); }

  /**
   * Create a region for an array of objects.
   * @param begin pointer to the first element of the array
   * @param size the number of elements in the array (the size of the
   *             whole region will be size * sizeof(T))
   * @param name the name of the region
   * @param t the type of the region
   * @param sub the subtype of the region
   */
  template< typename T >
  static Region array(T const *begin, unsigned long size, char const *name = 0,
                      Type t = No_mem, short sub = 0)
  {
    return Region::start_size((l4_addr_t)begin, sizeof(T) * size,
                              name, t, sub);
  }

  /** Get the start address. */
  unsigned long long begin() const { return _begin; }
  /** Get the address of the last byte. */
  unsigned long long end() const { return _end; }
  /** Set the start address. */
  void begin(unsigned long long b) { _begin = b; }
  /** Set the address of the last byte. */
  void end(unsigned long long e) { _end = e; }
  /** Get the name of the region. */
  char const *name() const { return _name; }
  /** Get size of the region */
  unsigned long long size() const { return _end - _begin + 1; }
  /** Set the name of the region. */
  void name(char const *name) { _name = name; }
  /** Get the type of the region. */
  Type type() const { return (Type)(_t); }
  /** Get the subtype of the region. */
  short sub_type() const { return _s; }
  /** Set the subtype of the region. */
  void sub_type(short s) { _s = s; }

  /** Print the region [begin; end] */
  void print() const;

  /** Print the region verbose (with name and type). */
  void vprint() const;

  /** Compare two regions. */
  bool operator < (Region const &o) const
  { return end() < o.begin(); }

  /** Check for an overlap. */
  bool overlaps(Region const &o) const
  { return !(*this < o) && !(o < *this); }

  /** Test if o is a sub-region of ourselves. */
  bool contains(Region const &o) const
  { return begin() <= o.begin() && end() >= o.end(); }

  /** Calculate the intersection. */
  Region intersect(Region const &o) const
  {
    if (!overlaps(o))
      return Region(No_mem);

    return Region(begin() > o.begin() ? begin() : o.begin(),
	end() < o.end() ? end() : o.end(),
	name(), type(), sub_type());
  }

  /** Check if the region is invalid */
  bool invalid() const { return begin()==0 && end()==0; }

private:
  unsigned long long _begin, _end;
  char const *_name;
  short _t, _s;
};


/** List of memory regions, based on an fixed size array. */
class Region_list
{
public:
  /**
   * Initialize the region list, using the array of given size
   * as backing store.
   */
  void init(Region *store, unsigned size,
            const char *name,
            unsigned long long max_combined_size = ~0ULL,
            unsigned long long address_limit = ~0ULL)
  {
    _reg = _end = store;
    _max = _reg + size;
    _name = name;
    _max_combined_size = max_combined_size;
    _address_limit = address_limit;
    _combined_size = 0;
  }

  /** Helper template for array parameters
   *
   * This helper allows to access the type and the size of compile-time
   * bound arrays. In particular used as parameters.
   */
  template< typename T >
    struct Array_type_helper;

  template< typename T, unsigned SZ >
    struct Array_type_helper<T[SZ]> {
      enum { size = SZ };
      typedef T type;
    };

  /**
   * Initialize the region list, using a region array as store
   *
   * @param store the array for the regoion backing store.
   * @param name the name of the region list for output
   * @param max_combined_size max sum of bytes in this list
   * @param address_limit maximum allowed address in this list
   */
  template<typename STORE>
  void init(STORE &store,
            const char *name,
            unsigned long long max_combined_size = ~0ULL,
            unsigned long long address_limit = ~0ULL)
  {
    init(store, Array_type_helper<STORE>::size, name,
         max_combined_size, address_limit);
  }

  /** Search for a region that overlaps o. */
  Region *find(Region const &o) const;

  /** Search for the region that contains o. */
  Region *contains(Region const &o);

  /**
   * Search for a memory region not overlapping any known region,
   * within search.
   */
  unsigned long long find_free(Region const &search,
                               unsigned long long size, unsigned align) const;

  /**
   * Search for a memory region not overlapping any know region, starting
   * from the end of the search region.
   */
  unsigned long long find_free_rev(Region const &search, unsigned long long _size,
                                   unsigned align) const;
  /**
   * Add a new region, with a upper limit check and verboseness.
   */
  void add(Region const &r, bool may_overlap = false);

  bool sub(Region const &r);

  /** Dump the whole region list. */
  void dump();

  /** Get the begin() iterator. */
  Region *begin() const { return _reg; }
  /** Get the end() iterator. */
  Region *end() const { return _end; }

  /** Remove the region given by the iterator r. */
  Region *remove(Region *r);

  /** Sort the region list (does bubble sort). */
  void sort();

  /** Optimize the region list.
   * Basically merges all regions with the same type, subtype, and name
   * that have a begin and end address on the same memory page.
   */
  void optimize();

protected:
  Region *_end;
  Region *_max;
  Region *_reg;

  const char *_name;
  unsigned long long _max_combined_size;
  unsigned long long _address_limit;
  unsigned long long _combined_size;

private:
  void swap(Region *a, Region *b);

  /**
   * Add a new memory region to the list. The new region must not overlap
   * any known region.
   */
  void add_nolimitcheck(Region const &r, bool may_overlap = false);
};

#endif

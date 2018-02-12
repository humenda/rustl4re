/*
 * (c) 2010 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *          Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include <l4/sys/icu>

#include <l4/vbus/vbus_types.h>
#include <vector>

#include <l4/re/dataspace>
#include <l4/re/util/cap_alloc>
#include <l4/re/util/unique_cap>
#include <l4/re/rm>

#include "res.h"

class Resource;
class Device;


class Resource_list : public std::vector<Resource *>
{
public:
  /**
   * \brief Find a resource by their ID.
   * \param id  The resource id (usually 4 letters ASCII, little endian
   *            encoded).
   * \return The first resource with the given ID, or null_ptr if non found.
   */
  Resource *find(l4_uint32_t id) const;

  /**
   * \brief Find a resource by their ID (using a 4-letter string).
   * \param id  The resource id (usually up to 4 letters ASCII).
   * \return The first resource with the given ID, or null_ptr if non found.
   */
  Resource *find(char const *id) const;
};

class Resource_space
{
public:
  virtual bool request(Resource *parent, Device *pdev,
                       Resource *child, Device *cdev) = 0;
  virtual void assign(Resource *parent, Resource *child) = 0;
  virtual bool alloc(Resource *parent, Device *pdev,
                     Resource *child, Device *cdev, bool resize) = 0;
  virtual bool adjust_children(Resource *self) = 0;
  virtual ~Resource_space() noexcept = 0;
};

inline Resource_space::~Resource_space() noexcept {}

class Resource
{
private:
  unsigned long _f;
  l4_uint32_t _id;
  Resource *_p;

public:
  typedef l4_uint64_t Addr;
  typedef l4_int64_t Size;

  enum Type
  {
    Invalid_res = L4VBUS_RESOURCE_INVALID,
    Irq_res     = L4VBUS_RESOURCE_IRQ,
    Mmio_res    = L4VBUS_RESOURCE_MEM,
    Io_res      = L4VBUS_RESOURCE_PORT,
    Bus_res     = L4VBUS_RESOURCE_BUS,
    Gpio_res    = L4VBUS_RESOURCE_GPIO,
    Dma_domain_res = L4VBUS_RESOURCE_DMA_DOMAIN
  };

  enum Flags
  {
    F_type_mask    = 0x00ff,
    F_disabled     = 0x0100,
    F_hierarchical = 0x0200,
    F_prefetchable = 0x0400,
    F_size_aligned = 0x0800,
    F_empty        = 0x1000,
    F_rom          = 0x2000,
    F_can_resize   = 0x4000,
    F_can_move     = 0x8000,

    F_width_64bit   = 0x010000,
    F_cached_mem    = 0x020000,
    F_relative      = 0x040000,

    Irq_type_base         = 0x100000,
    Irq_type_mask         = L4_IRQ_F_MASK       * Irq_type_base,
    Irq_type_none         = L4_IRQ_F_NONE       * Irq_type_base,
    Irq_type_level_high   = L4_IRQ_F_LEVEL_HIGH * Irq_type_base,
    Irq_type_level_low    = L4_IRQ_F_LEVEL_LOW  * Irq_type_base,
    Irq_type_raising_edge = L4_IRQ_F_POS_EDGE   * Irq_type_base,
    Irq_type_falling_edge = L4_IRQ_F_NEG_EDGE   * Irq_type_base,
    Irq_type_both_edges   = L4_IRQ_F_BOTH_EDGE  * Irq_type_base,
  };

  bool irq_is_level_triggered() const
  { return (_f & Irq_type_mask) & (L4_IRQ_F_LEVEL * Irq_type_base); }

  bool irq_is_low_polarity() const
  { return (_f & Irq_type_mask) & (L4_IRQ_F_NEG   * Irq_type_base); }

  explicit Resource(unsigned long flags = 0)
  : _f(flags), _id(0), _p(0), _s(0), _e(0), _a(0) {}

  Resource(unsigned long flags, Addr start, Addr end)
  : _f(flags), _id(0), _p(0), _s(start), _e(end), _a(end - start)
  {}

  Resource(unsigned type, unsigned long flags, Addr start, Addr end)
  : _f((type & F_type_mask) | (flags & ~(unsigned long)F_type_mask)),
    _id(0), _p(0), _s(start), _e(end), _a(end - start)
  {}

  Resource(char const *id, unsigned type, Addr start, Addr end)
  : _f(type), _id(str_to_id(id)), _p(0), _s(start), _e(end), _a(end - start)
  {}

  unsigned long flags() const { return _f; }
  void add_flags(unsigned long flags) { _f |= flags; }
  void del_flags(unsigned long flags) { _f &= ~flags; }
  bool hierarchical() const { return _f & F_hierarchical; }
  bool disabled() const { return _f & F_disabled; }
  bool prefetchable() const { return _f & F_prefetchable; }
  bool cached_mem() const { return _f & F_cached_mem; }
  bool empty() const { return _f & F_empty; }
  bool fixed_addr() const { return !(_f & F_can_move); }
  bool fixed_size() const { return !(_f & F_can_resize); }
  bool relative() const { return _f & F_relative; }
  unsigned type() const { return _f & F_type_mask; }

  virtual bool lt_compare(Resource const *o) const
  { return end() < o->start(); }

  static l4_uint32_t str_to_id(char const *id)
  {
    l4_uint32_t res = 0;
    for (unsigned i = 0; i < 4 && id && id[i]; ++i)
      res |= (l4_uint32_t)id[i] << (8 * i);
    return res;
  }

  void set_id(l4_uint32_t id)
  { _id = id; }

  void set_id(char const *id)
  { _id = str_to_id(id); }

  l4_uint32_t id() const { return _id; }


public:
//private:
  void set_empty(bool empty)
  {
    if (empty)
      _f |= F_empty;
    else
      _f &= ~F_empty;
  }

public:
  void disable() { _f |= F_disabled; }
  void enable()  { _f &= ~F_disabled; }

  virtual Resource_space *provided() const { return 0; }

  void dump(char const *type, int indent) const;
  virtual void dump(int indent = 0) const;

  virtual bool compatible(Resource *consumer, bool pref = true) const
  {
    if (type() != consumer->type())
      return false;

    return prefetchable() == (consumer->prefetchable() && pref);
  }

  Resource *parent() const { return _p; }
  void parent(Resource *p) { _p = p; }

  virtual ~Resource() = default;

private:
  Addr _s, _e;
  l4_umword_t _a;

  void _start_end(Addr s, Addr e) { _s = s; _e = e; }

public:
  void set_empty() { _s = _e = 0; set_empty(true); }
  void alignment(Size a)
  {
    _a = a;
    del_flags(F_size_aligned);
  }

  bool valid() const { return flags() && _s <= _e; }

  void validate()
  {
    if (!valid())
      disable();
  }

  Addr start() const { return _s; }
  Addr end() const { return _e; }
  Size size() const { return (Size)_e + 1 - _s; }

  bool contains(Resource const &o) const
  { return start() <= o.start() && end() >= o.end(); }

  void start(Addr start) { _e = start + (_e - _s); _s = start; }
  void end(Addr end)
  {
    _e = end;
    set_empty(false);
  }

  void size(Size size)
  {
    _e = _s - 1 + size;
    set_empty(false);
  }

  void start_end(Addr start, Addr end)
  {
    _start_end(start, end);
    set_empty(false);
  }

  void start_size(Addr start, Size s)
  {
    _start_end(start, start - 1 + s);
    set_empty(false);
  }

  bool is_64bit() const { return flags() & F_width_64bit; }

  l4_umword_t alignment() const
  {
    return  flags() & F_size_aligned ? (_e - _s) : _a;
  }

  virtual l4_addr_t map_iomem() const
  {
    if (type() != Mmio_res)
      return 0;
    return res_map_iomem(start(), size());
  }

  virtual l4vbus_device_handle_t provider_device_handle() const
  { return ~0; }
};

class Resource_provider : public Resource
{
private:
  class _RS : public Resource_space
  {
  private:
    typedef Resource::Addr Addr;
    typedef Resource::Size Size;
    Resource_list _rl;

  public:
    bool request(Resource *parent, Device *pdev, Resource *child, Device *cdev);
    bool alloc(Resource *parent, Device *pdev, Resource *child, Device *cdev,
               bool resize);
    void assign(Resource *parent, Resource *child);
    bool adjust_children(Resource *self);
  };

  mutable _RS _rs;

public:
  explicit Resource_provider(unsigned long flags)
  : Resource(flags), _rs() {}

  Resource_provider(unsigned long flags, Addr s, Addr e)
  : Resource(flags, s, e), _rs() {}

  Resource_space *provided() const
  { return &_rs; }
};

class Root_resource : public Resource
{
private:
  Resource_space *_rs;

public:
  Root_resource(unsigned long flags, Resource_space *rs)
  : Resource(flags), _rs(rs) {}

  Resource_space *provided() const { return _rs; }
  void dump(int) const {}
};


class Mmio_data_space : public Resource
{
private:
  L4Re::Util::Unique_cap<L4Re::Dataspace> _ds_ram;

public:
  L4Re::Rm::Unique_region<l4_addr_t> _r;

  Mmio_data_space(Size size, unsigned long alloc_flags = 0)
  : Resource(Mmio_res, 0, size - 1)
  {
    alloc_ram(size, alloc_flags);
  }

  void alloc_ram(Size size, unsigned long alloc_flags);

  l4_addr_t map_iomem() const
  {
    return _r.get();
  }
};

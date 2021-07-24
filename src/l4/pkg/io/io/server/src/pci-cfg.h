/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2010-2020 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *            Alexander Warg <warg@os.inf.tu-dresden.de>
 */
#pragma once

#include <l4/cxx/type_traits>
#include <l4/sys/l4int.h>

#include <cstdio>

namespace Hw { namespace Pci {

enum Cfg_width
{
  Cfg_long  = 2,
  Cfg_short = 1,
  Cfg_byte  = 0,
};

inline Cfg_width cfg_w_to_o(int width)
{
  switch (width)
    {
    default:
    case 32: return Cfg_long;
    case 16: return Cfg_short;
    case 8:  return Cfg_byte;
    }
}

template<unsigned S> struct _cfg_width;
template<> struct _cfg_width<1> { static const Cfg_width width = Cfg_byte; };
template<> struct _cfg_width<2> { static const Cfg_width width = Cfg_short; };
template<> struct _cfg_width<4> { static const Cfg_width width = Cfg_long; };
template<typename T> struct cfg_width { static const Cfg_width width = _cfg_width<sizeof(T)>::width; };

template< Cfg_width w > struct Cfg_type;
template<> struct Cfg_type<Cfg_byte>  { typedef l4_uint8_t Type; };
template<> struct Cfg_type<Cfg_short> { typedef l4_uint16_t Type; };
template<> struct Cfg_type<Cfg_long>  { typedef l4_uint32_t Type; };

/**
 * Mixin for PCI config space accessors to allow typed
 * read and write.
 */
template<typename B>
class Cfg_rw_mixin
{
private:
  B *_this() { return static_cast<B *>(this); }
  B const *_this() const { return static_cast<B const *>(this); }

public:
  template<typename T>
  int read(unsigned offset, T *val) const
  {
    union
    {
      l4_uint32_t v;
      T t;
    } x;

    int r = _this()->read(offset, &x.v, cfg_width<T>::width);
    *val = x.t;
    return r;
  }

  template<typename T> T read(unsigned offset) const
  {
    T v;
    read(offset, &v);
    return v;
  }

  template<typename T>
  int write(unsigned offset, T const &val) const
  {
    union
    {
      l4_uint32_t v;
      T t;
    } x;

    x.t = val;
    return _this()->write(offset, x.v, cfg_width<T>::width);
  }

  /**
   * Read for special 'config register' type data types.
   *
   * 'config register' data types have a constexpr member Ofs, that
   * defines the config space offset of the given register and a single
   * 8, 16, or 32 bit data member that stores the data of the config
   * config space register.
   */
  template<typename T>
  typename cxx::enable_if<(T::Ofs >= 0), T>::type
  read() const
  {  return this->read<T>(T::Ofs);  }

  /**
   * Write for special 'config register' type data types.
   *
   * 'config register' data types have a constexpr member Ofs, that
   * defines the config space offset of the given register and a single
   * 8, 16, or 32 bit data member that stores the data of the config
   * config space register.
   */
  template<typename T>
  typename cxx::enable_if<(T::Ofs >= 0), int>::type
  write(T const &val)
  { return this->write(T::Ofs, val); }
};


/**
 * Address in PCI / PCI Express config space.
 */
class Cfg_addr
{
public:

  // we store the config space address as specified in the PCI Express spec
  Cfg_addr(unsigned char bus, unsigned char dev, unsigned char fn, unsigned reg)
  : _a(  ((l4_uint32_t)bus << 20)
       | ((l4_uint32_t)dev << 15)
       | ((l4_uint32_t)fn  << 12)
       | ((l4_uint32_t)reg))
  {}

  Cfg_addr(Cfg_addr const &dev, unsigned reg)
  : _a((dev._a & ~0xfff) | (reg & 0xfff))
  {}

  l4_uint32_t to_compat_addr() const
  { return (_a & 0xff) | ((_a >> 4) & 0xffff00); }

  l4_uint32_t reg_offs(Cfg_width w = Cfg_byte) const
  { return (_a & 3) & (~0UL << (unsigned long)w); }

  unsigned bus() const { return (_a >> 20) & 0xff; }
  unsigned dev() const { return (_a >> 15) & 0x1f; }
  unsigned fn() const { return (_a >> 12) & 0x7; }
  unsigned devfn() const { return (_a >> 12) & 0xff; }
  unsigned reg() const { return _a & 0xfff; }

  Cfg_addr operator + (unsigned reg_offs) const
  { return Cfg_addr(_a + reg_offs); }

  l4_uint32_t addr() const { return _a; }
  Cfg_addr base() const { return Cfg_addr(bus(), dev(), fn(), 0); }

private:
  explicit Cfg_addr(l4_uint32_t raw) : _a(raw) {}

  l4_uint32_t _a;
};


/**
 * Interface for accesses to a PCI config space.
 */
class Config_space
{
public:
  virtual int cfg_read(Cfg_addr addr, l4_uint32_t *value, Cfg_width) = 0;
  virtual int cfg_write(Cfg_addr addr, l4_uint32_t value, Cfg_width) = 0;
  virtual ~Config_space() = 0;
};

inline Config_space::~Config_space() = default;


/**
 * Encapsulate the config space of a PCI device.
 *
 * This class is used to access the config space of the PCI bus.
 */
class Config : public Cfg_rw_mixin<Config>
{
public:
  enum Regs
  {
    /* Generic config */
    Vendor         = 0x00,
    Device         = 0x02,
    Command        = 0x04,
    Status         = 0x06,
    Class_rev      = 0x08,
    Cacheline_size = 0x0c,
    Latency_timer  = 0x0d,
    Header_type    = 0x0e,
    BIST           = 0x0f,

    /* Header type 0 config, normal PCI devices */
    Bar_0          = 0x10,
    Cardbus_cis    = 0x28,
    Subsys_vendor  = 0x2c,
    Subsys         = 0x2e,
    Rom_address    = 0x30,
    Capability_ptr = 0x34,
    Irq_line       = 0x3c,
    Irq_pin        = 0x3d,
    Min_gnt        = 0x3e,
    Max_lat        = 0x3f,

    /* Header type 1, PCI-PCI bridges */
    Primary           = 0x18,
    Secondary         = 0x19,
    Subordinate       = 0x1a,
    Secondary_latency = 0x1b,
    Io_base           = 0x1c,
    Io_limit          = 0x1d,
    Secondary_status  = 0x1e,
    Mem_base          = 0x20,
    Mem_limit         = 0x22,
    Pref_mem_base     = 0x24,
    Pref_mem_limit    = 0x26,
    Pref_mem_base_hi  = 0x28,
    Pref_mem_limit_hi = 0x2c,
    Io_base_hi        = 0x30,
    Io_limit_hi       = 0x32,
    Rom_address_1     = 0x38,
    Bridge_control    = 0x3e,

    /* header type 2, cardbus bridge */
    Cb_capability_ptr   = 0x14,
    Cb_secondary_status = 0x16,
    Cb_primary          = 0x18,
    Cb_cardbus          = 0x19,
    Cb_subordinate      = 0x1a,
    Cb_latency_timer    = 0x1b,
    Cb_mem_base_0       = 0x1c,
    Cb_mem_limit_0      = 0x20,
    Cb_mem_base_1       = 0x24,
    Cb_mem_limit_1      = 0x28,
    Cb_io_base_0        = 0x2c,
    Cb_io_base_0_hi     = 0x2e,
    Cb_io_limit_0       = 0x30,
    Cb_io_limit_0_hi    = 0x32,
    Cb_io_base_1        = 0x34,
    Cb_io_base_1_hi     = 0x36,
    Cb_io_limit_1       = 0x38,
    Cb_io_limi_1_hi     = 0x3a,
    Cb_bridge_control   = 0x3e,
    Cb_subsystem_vendor = 0x40,
    Cb_subsystem        = 0x42,
    Cb_legacy_mode_base = 0x44,
  };

  Config(Cfg_addr addr, Config_space *cfg) : _cfg(cfg), _addr(addr) {}
  Config() : _addr(0, 0, 0, 0) {}

  bool is_valid() const { return _cfg; }

  int read(unsigned reg, l4_uint32_t *value, Cfg_width w) const
  { return _cfg->cfg_read(_addr + reg, value, w); }

  using Cfg_rw_mixin<Config>::read;

  int write(unsigned reg, l4_uint32_t value, Cfg_width w) const
  { return _cfg->cfg_write(_addr + reg, value, w); }

  using Cfg_rw_mixin<Config>::write;

  Config operator + (unsigned offset) const
  { return Config(_addr + offset, _cfg); }

  Config_space *cfg_spc() const { return _cfg; }
  Cfg_addr addr() const { return _addr; }
  unsigned reg() const { return _addr.reg(); }

private:
  Config_space *_cfg = nullptr;
  Cfg_addr _addr;
};


/**
 * Generic PCI capability structure.
 */
class Cap : public Config
{
public:
  enum Types
  {
    Pm    = 0x01,
    Msi   = 0x05,
    Pcix  = 0x07,
    Vndr  = 0x09,
    Pcie  = 0x10,
    Msi_x = 0x11
  };

  l4_uint8_t id()
  {
    l4_uint8_t r;
    read(0, &r);
    return r;
  }

  Cap next()
  {
    l4_uint8_t r;
    read(1, &r);
    if (r)
      return Cap(Config(addr().base() + r, cfg_spc()));

    return Cap();
  }

  Cap() = default;
  Cap(Config const &cfg) : Config(cfg) {}
};

/**
 * Helper class to parse normal PCI type 0 BARs.
 *
 * This class is usually used as temporary object.
 */
class Cfg_bar : public Config
{
private:
  l4_uint64_t _base; ///< base address from the BAR
  l4_uint64_t _size; ///< size of the BAR
  l4_uint8_t _type;  ///< type
  l4_uint8_t _flags; ///< flags

public:
  enum Type
  {
    T_mmio, T_io
  };

  Cfg_bar() = default;
  Cfg_bar(Config const &c) : Config(c) {}

  /**
   * Parse a PCI BAR register (including size etc.)
   *
   * \pre must be called with disabled decoders
   */
  bool parse()
  {
    auto v = read<l4_uint32_t>(0);
    // do the BAR sizing
    write<l4_uint32_t>(0, ~(l4_uint32_t)0);
    auto s = read<l4_uint32_t>(0);
    write(0, v); // restore

    if (s == 0) // undefined
      return false;

    if ((s & 1) == 1) // IO
      {
        _type = T_io;
        _base = v & ~(l4_uint32_t)3;
        _size = (~(s & ~(l4_uint32_t)3) + 1) & s;
        _flags = 0;
        return true;
      }

    // MMIO
    _base = (v & ~(l4_uint32_t)0xf);
    _type = T_mmio;
    _flags = s & 0x8; // prefetchable

    if ((s & 0x7) == 0x4) // 64bit MMIO
      {
        auto u = read<l4_uint32_t>(4);
        write<l4_uint32_t>(4, ~(l4_uint32_t)0);
        auto us = read<l4_uint32_t>(4);
        write(4, u);
        _base |= static_cast<l4_uint64_t>(u) << 32;
        _size = ~((static_cast<l4_uint64_t>(us) << 32) | (s & ~(l4_uint32_t)0xf)) + 1;
        _flags |= 1; // 64bit
      }
    else
      _size = ~(s & ~(l4_uint32_t)0xf) + 1;

    return true;
  }

  Type type() const { return static_cast<Type>(_type); }
  bool is_64bit() const { return _flags & 1; }
  bool is_prefetchable() const { return _flags & 8; }
  l4_uint64_t base() const  { return _base; }
  l4_uint64_t size() const { return _size; }
};

/**
 * Wrapper class to work with PCIe extended capabilities
 */
class Extended_cap : public Config
{
public:
  enum Types
  {
    Aer = 0x01, ///< Advanced Error Reporting
    Dsn = 0x03, ///< Device Serial Number
    Pbe = 0x04, ///< Power Budgeting
    Acs = 0x0d, ///< Access Control Services
    Ari = 0x0e, ///< Alternative Routing-ID
  };

  Extended_cap() = default;
  Extended_cap(Config const &cfg) : Config(cfg) {}

  bool is_valid() const
  {
    return id() != 0 && id() < 0xffff && version() > 0;
  }

  l4_uint32_t header() const
  {
    l4_uint32_t hdr;
    read(0, &hdr);
    return hdr;
  }

  l4_uint16_t id() const
  {
    l4_uint16_t id;
    read(0, &id);
    return id;
  }

  l4_uint8_t version() const
  {
    l4_uint8_t v;
    read(2, &v);
    return v & 0xf;
  }

  l4_uint16_t next() const
  {
    l4_uint16_t n;
    read(2, &n);
    return (n & 0xffc0) >> 4;
  }

  void dump() const
  {
    printf("Extended Cap: id=%x, version=%x, next=%x\n", id(), version(), next());
  }
};

}}

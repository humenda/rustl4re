/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2010-2020 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *            Alexander Warg <warg@os.inf.tu-dresden.de>
 */
#pragma once

#include <pci-cfg.h>
#include <pci-if.h>

#include <l4/cxx/hlist>

namespace Hw { namespace Pci {

class Saved_cap : public cxx::H_list_item_t<Saved_cap>
{
public:
  Saved_cap(l4_uint8_t type, unsigned pos) : _type(type), _reg(pos) {}
  virtual ~Saved_cap() = 0;
  void save(Config cfg) { _save(cfg + _reg); }
  void restore(Config cfg) { _restore(cfg + _reg); }

  l4_uint8_t type() const { return _type; }
  unsigned cap_offset() const { return _reg; }

private:
  l4_uint8_t _type;
  unsigned   _reg;

  virtual void _save(Config cfg) = 0;
  virtual void _restore(Config cfg) = 0;
};

class Saved_config
{
public:
  template<typename T>
  T read(unsigned reg)
  {
    switch (cfg_width<T>::width)
      {
      case Cfg_byte:  return _regs.b[reg];
      case Cfg_short: return _regs.h[reg >> 1];
      case Cfg_long:  return _regs.w[reg >> 2];
      }
  }

  void save(If *dev);
  void restore(If *dev);

  Saved_cap *find_cap(l4_uint8_t type);
  void add_cap(Saved_cap *cap)
  { _caps.push_front(cap); }

private:
  typedef cxx::H_list_t<Saved_cap> Cap_list;
  union
  {
    l4_uint32_t w[16];
    l4_uint16_t h[16*2];
    l4_uint8_t  b[16*4];
  } _regs;

  Cap_list _caps;
};

inline Saved_cap::~Saved_cap() = default;

} }

/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2010-2020 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *            Alexander Warg <warg@os.inf.tu-dresden.de>
 */
#pragma once

#include <pci-cfg.h>
#include <hw_device.h>
#include <irqs.h>

namespace Hw { namespace Pci {

/**
 * Abstract interface of a generic PCI device
 */
class If : public virtual Dev_feature
{
public:
  virtual Resource *bar(int) const = 0;
  virtual Resource *rom() const = 0;
  virtual bool supports_msi() const = 0;

  virtual int cfg_read(l4_uint32_t reg, l4_uint32_t *value, Cfg_width) = 0;
  virtual int cfg_write(l4_uint32_t reg, l4_uint32_t value, Cfg_width) = 0;

  virtual l4_uint32_t vendor_device_ids() const = 0;
  virtual l4_uint32_t class_rev() const = 0;
  virtual l4_uint32_t subsys_vendor_ids() const = 0;
  virtual l4_uint32_t recheck_bars(unsigned decoders) = 0;
  virtual l4_uint32_t checked_cmd_read() = 0;
  virtual l4_uint16_t checked_cmd_write(l4_uint16_t mask, l4_uint16_t cmd) = 0;
  virtual bool enable_rom() = 0;

  virtual void enable_bus_master() = 0;

  virtual unsigned bus_nr() const = 0;
  virtual unsigned devfn() const = 0;
  unsigned device_nr() const { return devfn() >> 3; }
  unsigned function_nr() const { return devfn() & 7; }
  virtual unsigned phantomfn_bits() const = 0;
  virtual Config_space *config_space() const = 0;
  virtual Io_irq_pin::Msi_src *get_msi_src() = 0;

  virtual ~If() = 0;

  Cfg_addr cfg_addr(unsigned reg = 0) const
  { return Cfg_addr(bus_nr(), device_nr(), function_nr(), reg); }

  Config config(unsigned reg = 0) const
  { return Config(cfg_addr(reg), config_space()); }
};

inline If::~If() = default;

class Bridge_if
{
protected:
  ~Bridge_if() = default;

public:
  virtual unsigned alloc_bus_number() = 0;
  virtual bool check_bus_number(unsigned bus) = 0;
  virtual bool ari_forwarding_enable() = 0;
};

class Transparent_msi
{
public:
  virtual l4_uint32_t filter_cmd_read(l4_uint32_t cmd) = 0;
  virtual l4_uint16_t filter_cmd_write(l4_uint16_t cmd, l4_uint16_t ocmd) = 0;
  virtual ~Transparent_msi() = default;
};


} }


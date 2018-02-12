/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#pragma once

namespace Hw {
  class Device;
}

class Io_config
{
public:
  virtual bool transparent_msi(Hw::Device *) const = 0;
  virtual bool legacy_ide_resources(Hw::Device *) const = 0;
  virtual bool expansion_rom(Hw::Device *) const = 0;
  virtual int verbose() const = 0;
  virtual ~Io_config() = 0;

  static Io_config *cfg;
};

inline Io_config::~Io_config() {}

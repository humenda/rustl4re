/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2010-2020 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *            Alexander Warg <warg@os.inf.tu-dresden.de>
 */
#pragma once

#include <pci-dev.h>

namespace Hw { namespace Pci {

class Driver
{
public:
  virtual int probe(Dev *) = 0;
  virtual ~Driver() = 0;

  bool register_driver_for_class(l4_uint32_t device_class);
  bool register_driver(l4_uint16_t vendor, l4_uint16_t device);
  static Driver* find(Dev *);
};

inline Driver::~Driver() = default;

} }

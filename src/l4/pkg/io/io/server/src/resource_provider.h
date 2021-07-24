// SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom
/*
 * Copyright (C) 2010 Technische Universität Dresden (Germany)
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *            Alexander Warg <warg@os.inf.tu-dresden.de>
 * Copyright (C) 2019 Kernkonzept GmbH.
 *
 * This file is distributed under the terms of the GNU General Public
 * License, version 2.  Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include "resource.h"

class Device;

class Resource_provider : public Resource
{
private:
  class _RS : public Resource_space
  {
  private:
    typedef Resource::Addr Addr;
    typedef Resource::Size Size;
    Resource_list _rl;

    Size min_align(Resource const *r) const
    {
      switch (r->type())
        {
        case Mmio_res: return L4_PAGESIZE - 1;
        case Io_res:   return 3;
        default:       return 0;
        }
    }


  public:
    bool request(Resource *parent, Device *pdev, Resource *child,
                 Device *cdev) override;
    bool alloc(Resource *parent, Device *pdev, Resource *child,
               Device *cdev, bool resize) override;
    void assign(Resource *parent, Resource *child) override;
    bool adjust_children(Resource *self) override;
  };

  mutable _RS _rs;

public:
  explicit Resource_provider(unsigned long flags)
  : Resource(flags), _rs() {}

  Resource_provider(unsigned long flags, Addr s, Addr e)
  : Resource(flags, s, e), _rs() {}

  Resource_space *provided() const override
  { return &_rs; }
};

/*
 * (c) 2011 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#pragma once

#include "resource.h"
#include "irqs.h"

namespace Hw {

class Msi_resource : public Resource, public Msi_irq_pin
{
public:
  Msi_resource(unsigned msi)
  : Resource(Resource::Irq_res | Resource::Irq_type_falling_edge, msi, msi)
  {}

};


}

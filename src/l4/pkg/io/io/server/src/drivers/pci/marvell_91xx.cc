/*
 * Copyright (C) 2016 Kernkonzept GmbH.
 * Author(s): Alexander Warg <alexander.warg@kernkonzept.com>
 *
 * This file is distributed under the terms of the GNU General Public
 * License, version 2.  Please see the COPYING-GPL-2 file for details.
 *
 * As a special exception, you may use this file as part of a free software
 * library without restriction.  Specifically, if other files instantiate
 * templates or use macros or inline functions from this file, or you compile
 * this file and link it with other files to produce an executable, this
 * file does not by itself cause the resulting executable to be covered by
 * the GNU General Public License.  This exception does not however
 * invalidate any other reasons why the executable file might be covered by
 * the GNU General Public License.
 */

#include "cfg.h"
#include "debug.h"
#include "pci.h"

namespace {

using namespace Hw::Pci;

struct Pci_marvell_91xx_quirk : Driver
{
  int probe(Dev *d)
  {
    d_printf(DBG_DEBUG,
             "Found Marvell 91xx SATA controller: use PCI function aliasing quirk\n");

    // These controllers initiate DMA and may be also MSI transactions
    // using the wrong PCI function (function 1) to trigger the correct
    // entries for DMA remapping and MSI remapping we simulate this by
    // using 3 bits for the PCIe phantom function which allows the device
    // to use all PCI function IDs for transactions.
    //
    // TODO: The 9123 controller also has an IDE controller as PCI
    // function 1 which we should disable here, however the infrastructure
    // for that is missing so the IO config file should make sure that
    // the IDE function of the controller is never assigned to any virtual
    // bus.
    d->set_phantomfn_bits(3);
    return 0;
  }
};

static Pci_marvell_91xx_quirk _pci_marvell_sata_quirk;

struct Init
{
  Init()
  {
    _pci_marvell_sata_quirk.register_driver(0x1b4b, 0x9123);
    _pci_marvell_sata_quirk.register_driver(0x1b4b, 0x9170);
  }
};

static Init init;

}

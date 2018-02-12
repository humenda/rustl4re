/*
 * (c) 2013 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include "pci.h"

bool
Hw::Pci::Dev::check_pme_status()
{
  if (!_pm_cap)
    return false;

  Pm_cap::Pmcsr pmcsr;
  cfg_read(Pm_cap::pmcsr_reg(_pm_cap), &pmcsr);

  if (!pmcsr.pme_status())
    return false;

  bool res = false;
  // clear PME# status flag
  pmcsr.pme_status() = 1;
  if (pmcsr.pme_enable())
    {
      pmcsr.pme_enable() = 0;
      res = true;
    }

  cfg_write(Pm_cap::pmcsr_reg(_pm_cap), pmcsr);
  return res;
}



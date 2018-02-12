-- vim:set ft=lua:
--
-- Copyright (C) 2014 Kernkonzept GmbH.
-- Author(s): Matthias Lange <matthias.lange@kernkonzept.com>
--
-- This file is distributed under the terms of the GNU General Public
-- License, version 2.  Please see the COPYING-GPL-2 file for details.
--
-- OMAP4 (OMAP4, Pandaboard)

local Hw = Io.Hw
local Res = Io.Res

Io.hw_add_devices(function()
  Property.hid = "System Bus";

  Scm_pad_core = Hw.Scm_omap(function()
    Property.hid = "Sysctrl_Padconf_Core";
    compatible = {"ti,omap4-scrm"};
    Resource.regs = Res.mmio(0x4a100000, 0x4a100fff);
  end);

  Scm_pad_wkup = Hw.Scm_omap(function()
    Property.hid = "Sysctrl_Padconf_Wkup";
    compatible = {"ti,omap4-scrm"};
    Resource.regs = Res.mmio(0x4a31e000, 0x4a31efff);
  end);

  GPIO = Hw.Device(function()
    Property.hid = "Omap44x GPIO";

    -- The offset values for the scm are taken from the OMAP4430 ES TRM
    -- chapter 18 - System Control Module, table 18-8 (wakeup pad configuration
    -- register fields) and table 18-9 (core pad configuration register fields)
    GPIO1 = Hw.Gpio_omap44x_chip(function()
      compatible = {"ti,omap4-gpio"};
      Resource.regs = Res.mmio(0x4a310000, 0x4a310fff);
      Resource.irq = Res.irq(61, Io.Resource.Irq_type_level_high);
      Property.hid = "gpio-omap44x-GPIO1";
      Property.scm =
      {
        Scm_pad_wkup,
        0x184, 0x186, 0x190, 0x192, 0x048, 0x050, 0x054, 0x05a,
        0x05c, 0x068, 0x06a, 0x1ae, 0x1b0, 0x1b2, 0x1b4, 0x1b6,
        0x1b8, 0x1ba, 0x1bc, 0x1be, 0x1c0, 0x1c2, 0x1c4, 0x1c6,
        0x1c8, 0x1ca, 0x1cc, 0x1ce, 0x1d0, 0x066, 0x056, 0x058
      };
    end);
    GPIO2 = Hw.Gpio_omap44x_chip(function()
      compatible = {"ti,omap4-gpio"};
      Resource.regs = Res.mmio(0x48055000, 0x48055fff);
      Resource.irq = Res.irq(62, Io.Resource.Irq_type_level_high);
      Property.hid = "gpio-omap44x-GPIO2";
      Property.scm =
      {
        Scm_pad_core,
        0x050, 0x052, 0x054, 0x056, 0x058, 0x05a, 0x05c, 0x05e,
        0x060, 0x062, 0x064, 0x064, 0x068, 0x06a, 0x06c, 0x06e,
        0x070, 0x072, 0x074, 0x076, 0x078, 0x07a, 0x07c, 0x07e,
        0x080, 0x040, 0x042, 0x086, 0x088, 0x08a, 0x08c, 0x098
      };
    end);
    GPIO3 = Hw.Gpio_omap44x_chip(function()
      compatible = {"ti,omap4-gpio"};
      Resource.regs = Res.mmio(0x48057000, 0x48057fff);
      Resource.irq = Res.irq(63, Io.Resource.Irq_type_level_high);
      Property.hid = "gpio-omap44x-GPIO3";
      Property.scm =
      {
        Scm_pad_core,
        0x09a, 0x09c, 0x09e, 0x0a0, 0x0a2, 0x0a4, 0x0a6, 0x0a8,
        0x0aa, 0x0ac, 0x0ae, 0x0b0, 0x0b2, 0x0b4, 0x0b6, 0x0b8,
        0x0ba, 0x0bc, 0x0be, 0x0c0, 0x0c2, 0x0c4, 0x0c6, 0x0c8,
        0x0ca, 0x0cc, 0x0ce, 0x0d0, 0x0d2, 0x0d4, 0x0d6, 0x0d8
      };
    end);
    GPIO4 = Hw.Gpio_omap44x_chip(function()
      compatible = {"ti,omap4-gpio"};
      Resource.regs = Res.mmio(0x48059000, 0x48059fff);
      Resource.irq = Res.irq(64, Io.Resource.Irq_type_level_high);
      Property.hid = "gpio-omap44x-GPIO4";
      Property.scm =
      {
        Scm_pad_core,
        0x0da, 0x0dc, 0x0de, 0x0e0, 0x0e2, 0x0e4, 0x0e6, 0x0e8,
        0x0ea, 0x0ec, 0x0ee, 0x0f0, 0x0f2, 0x0f4, 0x0f6, 0x0f8,
        0x0fa, 0x0fc, 0x0fe, 0x100, 0x102, 0x104, 0x10e, 0x110,
        0x112, 0x114, 0x116, 0x118, 0x11a, 0x11c, 0x11e, 0x120
      };
    end);
    GPIO5 = Hw.Gpio_omap44x_chip(function()
      compatible = {"ti,omap4-gpio"};
      Resource.regs = Res.mmio(0x4805b000, 0x4805bfff);
      Resource.irq = Res.irq(65, Io.Resource.Irq_type_level_high);
      Property.hid = "gpio-omap44x-GPIO5";
      Property.scm =
      {
        Scm_pad_core,
        0x126, 0x128, 0x12a, 0x12c, 0x12e, 0x130, 0x132, 0x134,
        0x136, 0x138, 0x13a, 0x13c, 0x13e, 0x140, 0x142, 0x144,
        0x146, 0x148, 0x14a, 0x14c, 0x14e, 0x150, 0x152, 0x154,
        0x156, 0x158, 0x15a, 0x15c, 0x15e, 0x160, 0x162, 0x164
      };
    end);
    GPIO6 = Hw.Gpio_omap44x_chip(function()
      compatible = {"ti,omap4-gpio"};
      Resource.regs = Res.mmio(0x4805d000, 0x4805dfff);
      Resource.irq = Res.irq(66, Io.Resource.Irq_type_level_high);
      Property.hid = "gpio-omap44x-GPIO6";
      Property.scm =
      {
        Scm_pad_core,
        0x166, 0x168, 0x16a, 0x16c, 0x16e, 0x170, 0x172, 0x174,
        0x176, 0x178, 0x17a, 0x17c, 0x17e, 0x180, 0x182, 0x188,
        0x18a, 0x18c, 0x18e, 0x044, 0x046, 0x19a, 0x19c, 0x1a0,
        0x1a2, 0x1a4, 0x1a6, 0x1a8, 0x1aa, 0x1ac, 0x1d2, 0x1d4
      };
    end);
  end);

end)

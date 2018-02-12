-- vim:set ft=lua:
--
-- (c) 2008-2009 Technische Universität Dresden
--          2014 Matthias Lange <matthias.lange@kernkonzept.com>
-- This file is part of TUD:OS and distributed under the terms of the
-- GNU General Public License 2.
-- Please see the COPYING-GPL-2 file for details.
--

-- OMAP3 (OMAP3EVM, Beagleboard)

local Hw = Io.Hw
local Res = Io.Res

Io.hw_add_devices(function()

  Scm = Hw.Scm_omap(function()
    Property.hid = "System Control";
    compatible = {"ti,omap3-scrm"};
    Resource.regs = Res.mmio(0x48002000, 0x48002fff);
  end);

  -- The values in this table are taken from the OMAP35x TRM chapter 7
  -- System Control Module, table 7-4 (pad configuration register fields)
  GPIO = Hw.Device(function()
    GPIO1 = Hw.Gpio_omap35x_chip(function()
      Property.hid = "gpio-omap35x-GPIO1";
      compatible = {"ti,omap3-gpio"};
      Resource.regs = Res.mmio(0x48310000, 0x48310fff);
      Resource.irq = Res.irq(29);
      Property.scm =
      {
        Scm,
        0x1e0, 0xa06, 0xa08, 0xa0c, 0xa0e, 0xa10, 0xa12, 0xa14,
        0xa16, 0xa18, 0xa1a, 0x424, 0x5d8, 0x5da, 0x5dc, 0x5de,
        0x5e0, 0x5e2, 0x5e4, 0x5e6, 0x5e8, 0x5ea, 0x5ec, 0x5ee,
        0x5f0, 0x5f2, 0x5f4, 0x5f6, 0x5f8, 0x5fa, 0xa08, 0xa26
      };
    end);
    GPIO2 = Hw.Gpio_omap35x_chip(function()
      Property.hid = "gpio-omap35x-GPIO2";
      compatible = {"ti,omap3-gpio"};
      Resource.regs = Res.mmio(0x49050000, 0x49050fff);
      Resource.irq = Res.irq(30);
      Property.scm =
      {
        Scm,
        0x23a,    -1, 0x07a, 0x07c, 0x07e, 0x080, 0x082, 0x084,
        0x086, 0x088, 0x08a, 0x08c, 0x09e, 0x0a0, 0x0a2, 0x0a4,
        0x0a6, 0x0a8, 0x0aa, 0x0ac, 0x0b0, 0x0b2, 0x0b4, 0x0b6,
        0x0b8, 0x0ba, 0x0bc, 0x0be, 0x0c6, 0x0c8, 0x0ca, 0x0ce
      };
    end);
    GPIO3 = Hw.Gpio_omap35x_chip(function()
      Property.hid = "gpio-omap35x-GPIO3";
      compatible = {"ti,omap3-gpio"};
      Resource.regs = Res.mmio(0x49052000, 0x49052fff);
      Resource.irq = Res.irq(31);
      Property.scm =
      {
        Scm,
        0x0d0, 0x0d2, 0x0d4, 0x0d6, 0x0d8, 0x0da, 0x0dc, 0x0de,
        0x0e0, 0x0e2, 0x0e4, 0x0e6, 0x0e8, 0x0ea, 0x0ac, 0x0ee,
        0x0f0, 0x0f2, 0x0f4, 0x0f6, 0x0f8, 0x0fa, 0x0fc, 0x0fe,
        0x100, 0x102, 0x104, 0x106, 0x108, 0x10a, 0x10c, 0x10e
      };
    end);
    GPIO4 = Hw.Gpio_omap35x_chip(function()
      Property.hid = "gpio-omap35x-GPIO4";
      compatible = {"ti,omap3-gpio"};
      Resource.regs = Res.mmio(0x49054000, 0x49054fff);
      Resource.irq = Res.irq(32);
      Property.scm =
      {
        Scm,
        0x110, 0x112, 0x114, 0x116, 0x118, 0x11a, 0x11c, 0x11e,
        0x120, 0x122, 0x124, 0x126, 0x128, 0x12a, 0x12c, 0x12e,
        0x134, 0x136, 0x138, 0x13a, 0x13c, 0x13e, 0x140, 0x142,
        0x144, 0x146, 0x148, 0x14a, 0x14c, 0x14e, 0x150, 0x152
      };
    end);
    GPIO5 = Hw.Gpio_omap35x_chip(function()
      Property.hid = "gpio-omap35x-GPIO5";
      compatible = {"ti,omap3-gpio"};
      Resource.regs = Res.mmio(0x49056000, 0x49056fff);
      Resource.irq = Res.irq(33);
      Property.scm =
      {
        Scm,
        0x154, 0x156, 0x158, 0x15a, 0x15c, 0x15e, 0x160, 0x162,
        0x164, 0x166, 0x168, 0x16a, 0x16c, 0x16e, 0x170, 0x172,
        0x174, 0x176, 0x178, 0x17a, 0x17c, 0x17e, 0x180, 0x182,
        0x184, 0x186, 0x188, 0x18a, 0x18c, 0x18e, 0x190, 0x192
      };
    end);
    GPIO6 = Hw.Gpio_omap35x_chip(function()
      Property.hid = "gpio-omap35x-GPIO6";
      compatible = {"ti,omap3-gpio"};
      Resource.regs = Res.mmio(0x49058000, 0x49058fff);
      Resource.irq = Res.irq(34);
      Property.scm =
      {
        Scm,
        0x194, 0x196, 0x198, 0x19a, 0x19c, 0x19e, 0x1a0, 0x130,
        0x1be, 0x1b0, 0x1c6, 0x1c8, 0x1ca, 0x1cc, 0x1ce, 0x1d0,
        0x1d2, 0x1d4, 0x1d6, 0x1d8, 0x1da, 0x1dc, 0x1de, 0x1c0,
        0x1c2, 0x1c4, 0x1e2, 0x238, 0x1b0, 0x1b4, 0x1b6, 0x1b8
      };
    end);
  end)

  prcm = Hw.Device(function()
    Property.hid = "Omap3 Clock Module";
    compatible = {"ti,omap3-cm"};
    Resource.regs = Res.mmio(0x48004000, 0x48007fff);
  end)

  dss = Hw.Device(function()
    Property.hid = "OMAP_LCD";
    compatible = {"ti,omap3-dss"};
    Resource.regs = Res.mmio(0x48050000, 0x480501ff);
    Child.dispc = Hw.Device(function()
      Property.hid = "Omap3 Display Controller";
      compatible = {"ti,omap3-dispc"};
      Resource.regs = Res.mmio(0x48050400, 0x480507ff);
      Resource.irq = Res.irq(25);
    end);
  end)

  i2c1 = Hw.Device(function()
    Property.hid = "omap3-i2c";
    compatible = {"ti,omap3-i2c"};
    Resource.regs = Res.mmio(0x48070000, 0x48070fff);
    Resource.irq = Res.irq(56);
  end)
end)

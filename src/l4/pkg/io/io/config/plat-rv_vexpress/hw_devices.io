-- vim:set ft=lua:
-- (c) 2014 Kernkonzept GmbH
-- This file is distributed under the terms of the
-- GNU General Public License 2.
-- Please see the COPYING-GPL-2 file for details.

local Res = Io.Res
local Hw = Io.Hw

Io.hw_add_devices(function()

  virtio_mmio3 = Hw.Device(function()
    compatible = {"virtio,mmio"};
    Resource.mem = Res.mmio(0x10013600, 0x100137ff);
    Resource.irq = Res.irq(75);
  end);

  CTRL = Hw.Device(function()
    Property.hid = "System Control";
    Resource.regs = Res.mmio(0x10000000, 0x10000fff);
  end);

  clcd = Hw.Device(function()
    Property.hid = "AMBA PL110";
    compatible = {"arm,pl111","arm,primecell"};
    Resource.regs = Res.mmio(0x10020000, 0x10020fff);
  end);

  kmi0 = Hw.Device(function()
    compatible = {"arm,pl050","arm,primecell"};
    Resource.regs = Res.mmio(0x10006000, 0x10006fff);
    Resource.irq = Res.irq(44);
  end);

  kmi1 = Hw.Device(function()
    compatible = {"arm,pl050","arm,primecell"};
    Resource.regs = Res.mmio(0x10007000, 0x10007fff);
    Resource.irq = Res.irq(45);
  end);
end)

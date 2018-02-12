-- vi:ft=lua
-- (c) 2008-2009 Technische Universität Dresden
-- This file is part of TUD:OS and distributed under the terms of the
-- GNU General Public License 2.
-- Please see the COPYING-GPL-2 file for details.

-- Device tree for ARM Realview Emulation Baseboard (ARM926EJ-S)
-- This device tree works with the Qemu 'realview-eb' machine

local Res = Io.Res
local Hw = Io.Hw

Io.hw_add_devices(function()

  CTRL = Hw.Device(function()
    Property.hid = "System Control";
    compatible = {"arm,sysctl"};
    Resource.regs = Res.mmio(0x10000000, 0x10000fff);
  end);

  LCD = Hw.Device(function()
    Property.hid = "AMBA PL110";
    compatible = {"arm,pl111","arm,primecell"};
    Resource.regs = Res.mmio(0x10020000, 0x10020fff);
  end);

  KBD = Hw.Device(function()
    Property.hid = "AMBA KMI Kbd";
    compatible = {"arm,pl050","arm,primecell"};
    Resource.irq = Res.irq(52);
    Resource.regs = Res.mmio(0x10006000, 0x10006fff);
  end)

  MOUSE = Hw.Device(function()
    Property.hid = "AMBA KMI mou";
    compatible = {"arm,pl050","arm,primecell"};
    Resource.regs = Res.mmio(0x10007000, 0x10007fff);
    Resource.irq = Res.irq(53);
  end);

  NIC = Hw.Device(function()
    Property.hid = "smc91x";
    compatible = {"smsc,lan9118"};
    Resource.regs = Res.mmio(0x4e000000, 0x4e000fff);
    Resource.irq = Res.irq(60);
  end);

  -- this device is only available on a real machine and
  -- not part of Qemu's machine emulation
  PCI0 = Hw.Pci_iomem_root_bridge(function()
    Property.hid = "PNP0A03";
    Property.iobase    = 0x62000000;
    Property.iosize    = 0x00010000;
    Property.dev_start = 0x64000000;
    Property.dev_end   = 0x6fffffff;
    Property.int_a     = 80;
    Property.int_b     = 81;
    Property.int_c     = 82;
    Property.int_d     = 83;
  end);

  MEM1 = Hw.Device(function()
    Property.hid = "foomem";
    Resource.mmio = Io.Mmio_data_space(0x10000, 0);
  end);
end)

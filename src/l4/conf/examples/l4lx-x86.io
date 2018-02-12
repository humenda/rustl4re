-- vi:ft=lua

local hw = Io.system_bus()

Io.add_vbusses
{
  input = Io.Vi.System_bus
  {
    ps2dev = wrap(hw:match("PNP0[3F]??"));
  };

  fbdrv = Io.Vi.System_bus
  {
    PCI0 = Io.Vi.PCI_bus_ident
    {
      dummy = Io.Vi.PCI_dummy_device();
      pci_gfx = wrap(hw:match("PCI/display"));
    };

    x1 = wrap(hw:match("BIOS"));
    x2 = wrap(hw:match("PNP0900"));
    x3 = wrap(hw:match("PNP0100"));
  };

  l4linux = Io.Vi.System_bus
  {
    -- Add a new virtual PCI root bridge
    PCI0 = Io.Vi.PCI_bus
    {
      pci_l4x = wrap(hw:match("PCI/network", "PCI/storage", "PCI/media"));
    };
  };
}

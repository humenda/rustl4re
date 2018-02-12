-- vim:set ft=lua:
-- configuration file for io

local hw = Io.system_bus()

Io.add_vbus("gui", Io.Vi.System_bus
{
  ps2 = wrap(hw:match("PNP0[3F]??"));
})

Io.add_vbus("fbdrv", Io.Vi.System_bus
{
  PCI0 = Io.Vi.PCI_bus_ident
  {
    dummy = Io.Vi.PCI_dummy_device();
    pci = wrap(hw:match("PCI/display"));
  };

  bios = wrap(hw:match("BIOS"));

  dev1 = wrap(hw:match("PNP0900"));
  dev2 = wrap(hw:match("PNP0100"));
})

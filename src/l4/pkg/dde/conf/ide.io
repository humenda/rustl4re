#-- This is a configuration snippet for PCI device selection

pciclient => new System_bus()
{
   pci0 => new PCI_bus()
   {
      pci_storage[] => wrap(hw-root.match("PCI/CC_01"));
   }

}

-- vi:ft=lua

local Res = Io.Res
local Hw = Io.Hw

Io.hw_add_devices(function()

  GPIO = Hw.Gpio_bcm2835_chip(function()
    compatible = {"brcm,bcm2835-gpio"};
    Property.hid = "gpio-bcm2835-GPIO";
    Property.pins = 54;
    Resource.regs = Res.mmio(0x20200000, 0x202000b4);
    Resource.int0 = Res.irq(49);
    Resource.int2 = Res.irq(51);
  end);

  MBOX = Hw.Device(function()
    Property.hid = "BCM2835_mbox";
    Resource.regs = Res.mmio(0x2000b880, 0x2000bfff);
  end);

  FB = Hw.Device(function()
    Property.hid = "BCM2835_fb";
    Resource.mem = Res.mmio(0x5c006000, 0x60005fff);
  end);

  BSC2 = Hw.Device(function()
    compatible = {"brcm,bcm2835-i2c"};
    Property.hid = "BCM2835_bsc2";
    Resource.regs = Res.mmio(0x20805000, 0x20805fff);
    Resource.irq = Res.irq(53, Io.Resource.Irq_type_raising_edge);
  end);
end)

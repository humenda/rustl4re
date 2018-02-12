-- vim:set ft=lua:
--
-- Copyright (C) 2016 Kernkonzept GmbH
-- Author(s): Matthias Lange <matthias.lange@kernkonzept.com>
--
-- This file is part of TUD:OS and distributed under the terms of the GNU
-- General Public License 2. Please see the COPYING-GPL-2 file for details.

-- NXP/Freescale i.MX6q

local Hw = Io.Hw
local Res = Io.Res

Io.hw_add_devices(function()

  usdhc1 = Io.Hw.Device(function()
    compatible = {"fsl,imx6q-usdhc"};
    Resource.reg0 = Res.mmio(0x2190000, 0x2193fff);
    Resource.irq0 = Res.irq(54, Io.Resource.Irq_type_level_high);
    Property.flags = Io.Hw_device_DF_dma_supported;
  end);

  usdhc3 = Io.Hw.Device(function()
    compatible = {"fsl,imx6q-usdhc"};
    Resource.reg0 = Res.mmio(0x2198000, 0x219bfff);
    Resource.irq0 = Res.irq(56, Io.Resource.Irq_type_level_high);
    Property.flags = Io.Hw_device_DF_dma_supported;
  end);

  ccm = Io.Hw.Device(function()
    compatible = "fsl,imx6q-ccm";
    Resource.reg0 = Res.mmio(0x20c4000, 0x20c7fff);
    Resource.irq0 = Res.irq(119, Io.Resource.Irq_type_level_high);
    Resource.irq1 = Res.irq(120, Io.Resource.Irq_type_level_high);
  end);

  anatop = Io.Hw.Device(function()
    compatible = {"fsl,imx6q-anatop"};
    Resource.reg0 = Res.mmio(0x20c8000, 0x20c8fff);
    Resource.irq0 = Res.irq(81, Io.Resource.Irq_type_level_high);
    Resource.irq1 = Res.irq(86, Io.Resource.Irq_type_level_high);
    Resource.irq2 = Res.irq(159, Io.Resource.Irq_type_level_high);
  end);

  iomuxc = Io.Hw.Device(function()
    compatible = {"fsl,imx6q-iomuxc"};
    Resource.reg0 = Res.mmio(0x20e0000, 0x20e3fff);
  end);

  gpio1 = Io.Hw.Device(function()
    compatible = {"fsl,imx6q-gpio,1", "fsl,imx6q-gpio", "fsl,imx35-gpio"};
    Resource.reg0 = Res.mmio(0x209c000, 0x209ffff);
    Resource.irq0 = Res.irq(98, Io.Resource.Irq_type_level_high);
    Resource.irq1 = Res.irq(99, Io.Resource.Irq_type_level_high);
  end);

  gpio2 = Io.Hw.Device(function()
    compatible = {"fsl,imx6q-gpio,2", "fsl,imx6q-gpio", "fsl,imx35-gpio"};
    Resource.reg0 = Res.mmio(0x20a0000, 0x20a3fff);
    Resource.irq0 = Res.irq(100, Io.Resource.Irq_type_level_high);
    Resource.irq1 = Res.irq(101, Io.Resource.Irq_type_level_high);
  end);

  gpio3 = Io.Hw.Device(function()
    compatible = {"fsl,imx6q-gpio,3", "fsl,imx6q-gpio", "fsl,imx35-gpio"};
    Resource.reg0 = Res.mmio(0x20a4000, 0x20a7fff);
    Resource.irq0 = Res.irq(102, Io.Resource.Irq_type_level_high);
    Resource.irq1 = Res.irq(103, Io.Resource.Irq_type_level_high);
  end);

  gpio4 = Io.Hw.Device(function()
    compatible = {"fsl,imx6q-gpio,4", "fsl,imx6q-gpio", "fsl,imx35-gpio"};
    Resource.reg0 = Res.mmio(0x20a8000, 0x20abfff);
    Resource.irq0 = Res.irq(104, Io.Resource.Irq_type_level_high);
    Resource.irq1 = Res.irq(105, Io.Resource.Irq_type_level_high);
  end);

  gpio5 = Io.Hw.Device(function()
    compatible = {"fsl,imx6q-gpio,5", "fsl,imx6q-gpio", "fsl,imx35-gpio"};
    Resource.reg0 = Res.mmio(0x20ac000, 0x20affff);
    Resource.irq0 = Res.irq(106, Io.Resource.Irq_type_level_high);
    Resource.irq1 = Res.irq(107, Io.Resource.Irq_type_level_high);
  end);

  gpio6 = Io.Hw.Device(function()
    compatible = {"fsl,imx6q-gpio,6", "fsl,imx6q-gpio", "fsl,imx35-gpio"};
    Resource.reg0 = Res.mmio(0x20b0000, 0x20b3fff);
    Resource.irq0 = Res.irq(108, Io.Resource.Irq_type_level_high);
    Resource.irq1 = Res.irq(109, Io.Resource.Irq_type_level_high);
  end);

--  gpio7 = Io.Hw.Device(function()
--    compatible = {"fsl,imx6q-gpio,7", "fsl,imx6q-gpio", "fsl,imx35-gpio"};
--    Resource.reg0 = Res.mmio(0x20bc000, 0x20bffff);
--    Resource.irq0 = Res.irq(110, Io.Resource.Irq_type_level_high);
--    Resource.irq1 = Res.irq(111, Io.Resource.Irq_type_level_high);
--  end);

  can1 = Io.Hw.Device(function()
    compatible = {"fsl,imx6q-flexcan"};
    Resource.reg0 = Res.mmio(0x2090000, 0x2093fff);
    Resource.irq0 = Res.irq(142, Io.Resource.Irq_type_level_high);
  end);

  can2 = Io.Hw.Device(function()
    compatible = {"fsl,imx6q-flexcan"};
    Resource.reg0 = Res.mmio(0x2094000, 0x2097fff);
    Resource.irq0 = Res.irq(143, Io.Resource.Irq_type_level_high);
  end);

  usbphy1 = Io.Hw.Device(function()
    compatible = {"fsl,imx6q-usbphy", "fsl,imx23-usbphy"};
    Resource.reg0 = Res.mmio(0x20c9000, 0x20c9fff);
    Resource.irq0 = Res.irq(76, Io.Resource.Irq_type_level_high);
  end);

  usbphy2 = Io.Hw.Device(function()
    compatible = {"fsl,imx6q-usbphy", "fsl,imx23-usbphy"};
    Resource.reg0 = Res.mmio(0x20ca000, 0x20cafff);
    Resource.irq0 = Res.irq(77, Io.Resource.Irq_type_level_high);
  end);

  usbotg = Io.Hw.Device(function()
    compatible = {"fsl,imx6q-usb", "fsl,imx27-usb"};
    Resource.reg0 = Res.mmio(0x2184000, 0x21841ff);
    Resource.irq0 = Res.irq(75,  Io.Resource.Irq_type_level_high);
  end);

  usbh1 = Io.Hw.Device(function()
    compatible = {"fsl,imx6q-usb", "fsl,imx27-usb"};
    Resource.reg0 = Res.mmio(0x2184200, 0x21843ff);
    Resource.irq0 = Res.irq(72, Io.Resource.Irq_type_level_high);
  end);

  usbmisc = Io.Hw.Device(function()
    compatible = {"fsl,imx6q-usbmisc"};
    Resource.reg0 = Res.mmio(0x2184800, 0x21849ff);
  end);

  i2c3 = Io.Hw.Device(function()
    compatible = {"fsl,imx6q-i2c", "fsl,imx21-i2c"};
    Resource.reg0 = Res.mmio(0x21a8000, 0x21abfff);
    Resource.irq0 = Res.irq(70, Io.Resource.Irq_type_level_high);
  end);

  ethernet = Io.Hw.Device(function()
    compatible = {"fsl,imx6q-fec"};
    Resource.reg0 = Res.mmio(0x2188000, 0x218bfff);
    Resource.irq0 = Res.irq(150, Io.Resource.Irq_type_level_high);
    Resource.irq1 = Res.irq(151, Io.Resource.Irq_type_level_high);
    Property.flags = Io.Hw_device_DF_dma_supported;
  end);

  apbhdma = Io.Hw.Device(function()
    compatible = {"fsl,imx6q-dma-apbh", "fsl,imx28-dma-apbh"};
    Resource.reg0 = Res.mmio(0x110000, 0x111fff);
    Resource.irq0 = Res.irq(45, Io.Resource.Irq_type_level_high);
  end);

  gpmi = Io.Hw.Device(function()
    compatible = {"fsl,imx6q-gpmi-nand"};
    Resource.reg0 = Res.mmio(0x112000, 0x113fff);
    Resource.reg1 = Res.mmio(0x114000, 0x117fff);
    Resource.irq0 = Res.irq(47, Io.Resource.Irq_type_level_high);
    Property.flags = Io.Hw_device_DF_dma_supported;
  end);

end)

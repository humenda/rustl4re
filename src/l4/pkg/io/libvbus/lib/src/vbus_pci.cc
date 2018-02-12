/*
 * (c) 2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *          Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/vbus/vbus_pci.h>
#include <l4/vbus/vbus_pci-ops.h>
#include <l4/vbus/vbus_generic>
#include <l4/cxx/ipc_stream>

int L4_CV
l4vbus_pci_cfg_read(l4_cap_idx_t vbus, l4vbus_device_handle_t handle,
                    l4_uint32_t bus, l4_uint32_t devfn,
                    l4_uint32_t reg, l4_uint32_t *value, l4_uint32_t width)
{
  L4::Ipc::Iostream s(l4_utcb());
  l4vbus_device_msg(handle, L4vbus_pciroot_cfg_read, s);
  s << bus << devfn << reg << width;
  int err = l4_error(s.call(vbus, L4vbus::Vbus::Protocol));
  if (err < 0)
    return err;
  s >> *value;
  return 0;
}

int L4_CV
l4vbus_pci_cfg_write(l4_cap_idx_t vbus, l4vbus_device_handle_t handle,
                     l4_uint32_t bus, l4_uint32_t devfn,
                     l4_uint32_t reg, l4_uint32_t value, l4_uint32_t width)
{
  L4::Ipc::Iostream s(l4_utcb());
  l4vbus_device_msg(handle, L4vbus_pciroot_cfg_write, s);
  s << bus << devfn << reg << value << width;
  return l4_error(s.call(vbus, L4vbus::Vbus::Protocol));
}

int L4_CV
l4vbus_pci_irq_enable(l4_cap_idx_t vbus, l4vbus_device_handle_t handle,
                      l4_uint32_t bus, l4_uint32_t devfn,
                      int pin, unsigned char *trigger, unsigned char *polarity)
{
  int irq;
  L4::Ipc::Iostream s(l4_utcb());
  l4vbus_device_msg(handle, L4vbus_pciroot_cfg_irq_enable, s);
  s << bus << devfn << pin;
  int err = l4_error(s.call(vbus, L4vbus::Vbus::Protocol));
  if (err < 0)
    return err;

  s >> irq >> *trigger >> *polarity;
  return irq;
}




int L4_CV
l4vbus_pcidev_cfg_read(l4_cap_idx_t vbus, l4vbus_device_handle_t handle,
                    l4_uint32_t reg, l4_uint32_t *value, l4_uint32_t width)
{
  L4::Ipc::Iostream s(l4_utcb());
  l4vbus_device_msg(handle, L4vbus_pcidev_cfg_read, s);
  s << reg << width;
  int err = l4_error(s.call(vbus, L4vbus::Vbus::Protocol));
  if (err < 0)
    return err;
  s >> *value;
  return 0;
}

int L4_CV
l4vbus_pcidev_cfg_write(l4_cap_idx_t vbus, l4vbus_device_handle_t handle,
                     l4_uint32_t reg, l4_uint32_t value, l4_uint32_t width)
{
  L4::Ipc::Iostream s(l4_utcb());
  l4vbus_device_msg(handle, L4vbus_pcidev_cfg_write, s);
  s << reg << value << width;
  return l4_error(s.call(vbus, L4vbus::Vbus::Protocol));
}

int L4_CV
l4vbus_pcidev_irq_enable(l4_cap_idx_t vbus, l4vbus_device_handle_t handle,
                         unsigned char *trigger, unsigned char *polarity)
{
  int irq;
  L4::Ipc::Iostream s(l4_utcb());
  l4vbus_device_msg(handle, L4vbus_pcidev_cfg_irq_enable, s);
  int err = l4_error(s.call(vbus, L4vbus::Vbus::Protocol));
  if (err < 0)
    return err;

  s >> irq >> *trigger >> *polarity;
  return irq;
}



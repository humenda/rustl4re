/*
 * (c) 2009 Torsten Frenzel <frenzel@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/cxx/ipc_stream>
#include <l4/vbus/vbus_generic>
#include <l4/vbus/vbus_gpio.h>
#include <l4/vbus/vdevice-ops.h>
#include <l4/vbus/vbus_gpio-ops.h>

int L4_CV
l4vbus_gpio_setup(l4_cap_idx_t vbus, l4vbus_device_handle_t handle,
                  unsigned pin, unsigned mode, int outvalue)
{
  L4::Ipc::Iostream s(l4_utcb());
  l4vbus_device_msg(handle, L4VBUS_GPIO_OP_SETUP, s);
  s << pin << mode << outvalue;
  return l4_error(s.call(vbus, L4vbus::Vbus::Protocol));
}

int L4_CV
l4vbus_gpio_config_pull(l4_cap_idx_t vbus, l4vbus_device_handle_t handle,
                        unsigned pin, unsigned mode)
{
  L4::Ipc::Iostream s(l4_utcb());
  l4vbus_device_msg(handle, L4VBUS_GPIO_OP_CONFIG_PULL, s);
  s << pin << mode;
  return l4_error(s.call(vbus, L4vbus::Vbus::Protocol));
}

int L4_CV
l4vbus_gpio_config_pad(l4_cap_idx_t vbus, l4vbus_device_handle_t handle,
                       unsigned pin, unsigned func, unsigned value)
{
  L4::Ipc::Iostream s(l4_utcb());
  l4vbus_device_msg(handle, L4VBUS_GPIO_OP_CONFIG_PAD, s);
  s << pin << func << value;
  return l4_error(s.call(vbus, L4vbus::Vbus::Protocol));
}

int L4_CV
l4vbus_gpio_config_get(l4_cap_idx_t vbus, l4vbus_device_handle_t handle,
                       unsigned pin, unsigned func, unsigned *value)
{
  L4::Ipc::Iostream s(l4_utcb());
  l4vbus_device_msg(handle, L4VBUS_GPIO_OP_CONFIG_GET, s);
  s << pin << func;
  int r = l4_error(s.call(vbus, L4vbus::Vbus::Protocol));
  if (r)
    return r;

  s >> *value;

  return 0;
}

int L4_CV
l4vbus_gpio_get(l4_cap_idx_t vbus, l4vbus_device_handle_t handle, unsigned pin)
{
  L4::Ipc::Iostream s(l4_utcb());
  l4vbus_device_msg(handle, L4VBUS_GPIO_OP_GET, s);
  s << pin;
  return l4_error(s.call(vbus, L4vbus::Vbus::Protocol));
}

int L4_CV
l4vbus_gpio_set(l4_cap_idx_t vbus, l4vbus_device_handle_t handle, unsigned pin, int value)
{
  L4::Ipc::Iostream s(l4_utcb());
  l4vbus_device_msg(handle, L4VBUS_GPIO_OP_SET, s);
  s << pin << value;
  return l4_error(s.call(vbus, L4vbus::Vbus::Protocol));
}

int L4_CV
l4vbus_gpio_multi_setup(l4_cap_idx_t vbus, l4vbus_device_handle_t handle,
                        unsigned offset, unsigned mask,
                        unsigned mode, unsigned outvalues)
{
  L4::Ipc::Iostream s(l4_utcb());
  l4vbus_device_msg(handle, L4VBUS_GPIO_OP_MULTI_SETUP, s);
  s << offset << mask << mode << outvalues;
  return l4_error(s.call(vbus, L4vbus::Vbus::Protocol));
}

int L4_CV
l4vbus_gpio_multi_config_pad(l4_cap_idx_t vbus, l4vbus_device_handle_t handle,
                             unsigned offset, unsigned mask,
                             unsigned func, unsigned value)
{
  L4::Ipc::Iostream s(l4_utcb());
  l4vbus_device_msg(handle, L4VBUS_GPIO_OP_MULTI_CONFIG_PAD, s);
  s << offset << mask << func << value;
  return l4_error(s.call(vbus, L4vbus::Vbus::Protocol));
}

int L4_CV
l4vbus_gpio_multi_get(l4_cap_idx_t vbus, l4vbus_device_handle_t handle,
                      unsigned offset, unsigned *data)
{
  L4::Ipc::Iostream s(l4_utcb());
  l4vbus_device_msg(handle, L4VBUS_GPIO_OP_MULTI_GET, s);
  s << offset;
  int err = l4_error(s.call(vbus, L4vbus::Vbus::Protocol));
  if (err >= 0 && data)
    s >> *data;
  return err;
}

int L4_CV
l4vbus_gpio_multi_set(l4_cap_idx_t vbus, l4vbus_device_handle_t handle,
                      unsigned offset, unsigned mask, unsigned data)
{
  L4::Ipc::Iostream s(l4_utcb());
  l4vbus_device_msg(handle, L4VBUS_GPIO_OP_MULTI_SET, s);
  s << offset << mask << data;
  return l4_error(s.call(vbus, L4vbus::Vbus::Protocol));
}

int L4_CV
l4vbus_gpio_to_irq(l4_cap_idx_t vbus, l4vbus_device_handle_t handle, unsigned pin)
{
  L4::Ipc::Iostream s(l4_utcb());
  l4vbus_device_msg(handle, L4VBUS_GPIO_OP_TO_IRQ, s);
  s << pin;
  return l4_error(s.call(vbus, L4vbus::Vbus::Protocol));
}


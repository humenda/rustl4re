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
#include <l4/vbus/vbus_i2c.h>
#include <l4/vbus/vdevice-ops.h>

int L4_CV
l4vbus_i2c_write(l4_cap_idx_t vbus, l4vbus_device_handle_t handle,
                 l4_uint16_t addr, l4_uint8_t sub_addr,
                 l4_uint8_t *buffer, unsigned long size)
{
  L4::Ipc::Iostream s(l4_utcb());
  l4vbus_device_msg(handle, L4vbus_i2c_write, s);
  s << addr << sub_addr << size;
  s << L4::Ipc::buf_cp_out(buffer, size);
  return l4_error(s.call(vbus, L4vbus::Vbus::Protocol));
}

int L4_CV
l4vbus_i2c_read(l4_cap_idx_t vbus, l4vbus_device_handle_t handle,
                l4_uint16_t addr, l4_uint8_t sub_addr,
                l4_uint8_t *buffer, unsigned long *size)
{
  L4::Ipc::Iostream s(l4_utcb());
  l4vbus_device_msg(handle, L4vbus_i2c_read, s);
  s << addr << sub_addr << *size;

  int err = l4_error(s.call(vbus, L4vbus::Vbus::Protocol));
  if (err)
    return err;
  s >> *size >> L4::Ipc::buf_cp_in(buffer, *size);
  return 0;
}

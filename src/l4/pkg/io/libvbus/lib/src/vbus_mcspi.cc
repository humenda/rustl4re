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
#include <l4/vbus/vbus_mcspi.h>
#include <l4/vbus/vdevice-ops.h>

int L4_CV
l4vbus_mcspi_read(l4_cap_idx_t vbus, l4vbus_device_handle_t handle,
                  unsigned channel, l4_umword_t *value)
{
  L4::Ipc::Iostream s(l4_utcb());
  l4vbus_device_msg(handle, L4vbus_mcspi_read, s);
  s << channel;
  int ret = l4_error(s.call(vbus, L4vbus::Vbus::Protocol));
  if (!ret)
    s >> *value;
  return ret;
}

int L4_CV
l4vbus_mcspi_write(l4_cap_idx_t vbus, l4vbus_device_handle_t handle,
                   unsigned channel, l4_umword_t value)
{
  L4::Ipc::Iostream s(l4_utcb());
  l4vbus_device_msg(handle, L4vbus_mcspi_write, s);
  s << channel << value;
  return l4_error(s.call(vbus, L4vbus::Vbus::Protocol));
}

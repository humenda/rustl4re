/*
 * (c) 2013 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/vbus/vbus_pm.h>
#include <l4/vbus/vbus_pm-ops.h>
#include <l4/vbus/vbus_generic>
#include <l4/cxx/ipc_stream>

int L4_CV
l4vbus_pm_suspend(l4_cap_idx_t vbus, l4vbus_device_handle_t handle)
{
  L4::Ipc::Iostream s(l4_utcb());
  l4vbus_device_msg(handle, L4VBUS_PM_OP_SUSPEND, s);
  int err = l4_error(s.call(vbus, L4vbus::Vbus::Protocol));
  return err;
}

int L4_CV
l4vbus_pm_resume(l4_cap_idx_t vbus, l4vbus_device_handle_t handle)
{
  L4::Ipc::Iostream s(l4_utcb());
  l4vbus_device_msg(handle, L4VBUS_PM_OP_RESUME, s);
  int err = l4_error(s.call(vbus, L4vbus::Vbus::Protocol));
  return err;
}


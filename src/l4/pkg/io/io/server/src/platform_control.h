/*
 * (c) 2014 Alexander Warg <warg@os.inf.tu-dresden.de>
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#pragma once

#include <l4/sys/platform_control>
#include "inhibitor_mux.h"
#include <l4/sys/cxx/ipc_epiface>

namespace Hw { class Root_bus; }

class Platform_control
: public Inhibitor_mux,
  public L4::Epiface_t<Platform_control, L4::Platform_control>
{
public:
  explicit Platform_control(Hw::Root_bus *hw_root)
  : _state(0), _hw_root(hw_root) {}

  void all_inhibitors_free(l4_umword_t id);
  void cancel_op() { _state &= ~Op_in_progress_mask; }

  int op_system_shutdown(L4::Platform_control::Rights, l4_umword_t extra)
  {
    if (extra)
      return start_operation(Reboot_in_progress);
    return start_operation(Shutdown_in_progress);
  }

  int op_system_suspend(L4::Platform_control::Rights, l4_umword_t)
  { return start_operation(Suspend_in_progress); }

  int op_cpu_enable(L4::Platform_control::Rights, l4_umword_t)
  { return -L4_ENOSYS; }

  int op_cpu_disable(L4::Platform_control::Rights, l4_umword_t)
  { return -L4_ENOSYS; }

private:
  enum State_bits
  {
    Suspend_in_progress  = 1,
    Shutdown_in_progress = 2,
    Reboot_in_progress   = 4,
    Op_in_progress_mask  = Suspend_in_progress
                           | Shutdown_in_progress
                           | Reboot_in_progress
  };

  l4_umword_t _state;
  Hw::Root_bus *_hw_root;


  unsigned in_progress_ops() const { return _state & Op_in_progress_mask; }
  int start_operation(unsigned op);
};

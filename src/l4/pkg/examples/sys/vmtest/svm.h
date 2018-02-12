/*
 * Copyright (C) 2014-2015 Kernkonzept GmbH.
 * Author(s): Steffen Liebergeld <steffen.liebergeld@kernkonzept.com>
 *
 * This file is distributed under the terms of the GNU General Public
 * License, version 2.  Please see the COPYING-GPL-2 file for details.
 */
#pragma once
#include "vm.h"

class Svm: public Vm
{
public:
  Svm()
  {
    setup_vm();
    vmcb_s = (l4_vm_svm_vmcb_t *)vmcb;
    is_vmx = 0;
  }

protected:
  bool cpu_virt_capable();
  bool npt_available();
  void initialize_vmcb(unsigned long_mode);
  unsigned handle_vmexit();
  void set_rax(l4_umword_t rax);
  void set_rsp(l4_umword_t rsp);
  void set_rflags(l4_umword_t rflags);
  void set_rip(l4_umword_t rip);
  void set_cr0(l4_umword_t cr0);
  void set_cr3(l4_umword_t cr3);
  void set_cr4(l4_umword_t cr4);
  void set_dr7(l4_umword_t dr7);
  void set_efer(l4_umword_t efer);
  l4_umword_t get_rax();
  void enable_npt();
  void disable_npt();

private:
  l4_vm_svm_vmcb_t *vmcb_s;
};


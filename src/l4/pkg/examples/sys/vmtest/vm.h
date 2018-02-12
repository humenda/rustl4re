/*
 * Copyright (C) 2014-2015 Kernkonzept GmbH.
 * Author(s): Steffen Liebergeld <steffen.liebergeld@kernkonzept.com>
 *
 * This file is distributed under the terms of the GNU General Public
 * License, version 2.  Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include <l4/sys/capability>
#include <l4/sys/vm>
#include <l4/sys/types.h>
#include <l4/sys/vcpu.h>

enum { STACK_SIZE = 8 << 10 }; // 8kb

// used to distinguish between intel and amd cpus
// reason: vmcall is vmmcall on amd
extern long is_vmx; // defined in guest.S, shared with the vm

enum Memory_layout
{
  Code  = 0x1000,
  Interrupt_handler = 0x2000,
  Flags = 0x3000, // contains is_vmx
  Stack = 0x4000,
  Idt   = 0x6000,
  Gdt   = 0x7000,
};

class Vm
{
public:
  void run_tests();

private:
  static void handler();
  virtual unsigned vm_resume();
  l4_umword_t make_ia32e_cr3();
  l4_umword_t make_ia32_cr3();

protected:
  void setup_vm();
  // to be implemented by svm/vmx classes
  virtual bool cpu_virt_capable() = 0;
  virtual bool npt_available() = 0;
  virtual void initialize_vmcb(unsigned long_mode) = 0;
  virtual unsigned handle_vmexit() = 0;
  virtual void set_rax(l4_umword_t rax) = 0;
  virtual void set_rsp(l4_umword_t rsp) = 0;
  virtual void set_rflags(l4_umword_t rflags) = 0;
  virtual void set_rip(l4_umword_t rip) = 0;
  virtual void set_cr0(l4_umword_t cr0) = 0;
  virtual void set_cr3(l4_umword_t cr3) = 0;
  virtual void set_cr4(l4_umword_t cr4) = 0;
  virtual void set_dr7(l4_umword_t dr7) = 0;
  virtual void set_efer(l4_umword_t efer) = 0;
  virtual l4_umword_t get_rax() = 0;
  virtual void enable_npt() = 0;
  virtual void disable_npt() = 0;
  l4_vcpu_state_t *vcpu;
  void *vmcb; // named vmcs on intel
  L4::Cap<L4::Vm> vm_cap;

  enum exit_reason
    {
      Legacy = 0x1,
      VMCALL = 0x2,
      Alignment_check_intercept,
      Exception_intercept
    };
};


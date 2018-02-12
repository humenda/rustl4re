/*
 * Copyright (C) 2014-2015 Kernkonzept GmbH.
 * Author(s): Steffen Liebergeld <steffen.liebergeld@kernkonzept.com>
 */
/*
 * Author(s): Adam Lackorzynski
 *            Alexander Warg
 *            Matthias Lange <mlange@sec.t-labs.tu-berlin.de>
 *
 * (c) 2008-2012 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include "vmx.h"
#include "vmcs.h"

#include <cstdio>
#include <cstdlib>

#include <l4/re/env>
#include <l4/sys/vm.h>
#include <l4/util/cpu.h>
#include <l4/sys/thread.h>
#include <l4/sys/kdebug.h>

static bool verbose = false;

bool
Vmx::cpu_virt_capable()
{
  l4_umword_t ax, bx, cx, dx;
  if (!l4util_cpu_has_cpuid())
    return false;

  l4util_cpu_cpuid(0x1, &ax, &bx, &cx, &dx);

  if (!(cx & (1 << 5)))
    {
      printf("# CPU does not support VMX.\n");
      return false;
    }

  return true;
}

void
Vmx::initialize_vmcb(unsigned long_mode)
{
  vmwrite(VMX_GUEST_CS_SEL, 0x8);
  vmwrite(VMX_GUEST_CS_ACCESS_RIGHTS, 0xd09b);
  vmwrite(VMX_GUEST_CS_LIMIT, 0xffffffff);
  vmwrite(VMX_GUEST_CS_BASE, 0);

  vmwrite(VMX_GUEST_SS_SEL, 0x28);
  vmwrite(VMX_GUEST_SS_ACCESS_RIGHTS, 0xc093);
  vmwrite(VMX_GUEST_SS_LIMIT, 0xffffffff);
  vmwrite(VMX_GUEST_SS_BASE, 0);

  vmwrite(VMX_GUEST_DS_SEL, 0x10);
  vmwrite(VMX_GUEST_DS_ACCESS_RIGHTS, 0xc093);
  vmwrite(VMX_GUEST_DS_LIMIT, 0xffffffff);
  vmwrite(VMX_GUEST_DS_BASE, 0);

  vmwrite(VMX_GUEST_ES_SEL, 0x0);
  vmwrite(VMX_GUEST_ES_ACCESS_RIGHTS, 0x14003);
  vmwrite(VMX_GUEST_ES_LIMIT, 0xffffffff);
  vmwrite(VMX_GUEST_ES_BASE, 0);

  vmwrite(VMX_GUEST_FS_SEL, 0x0);
  vmwrite(VMX_GUEST_FS_ACCESS_RIGHTS, 0x1c0f3);
  vmwrite(VMX_GUEST_FS_LIMIT, 0xffffffff);
  vmwrite(VMX_GUEST_FS_BASE, 0);

  vmwrite(VMX_GUEST_GS_SEL, 0x0);
  vmwrite(VMX_GUEST_GS_ACCESS_RIGHTS, 0x1c0f3);
  vmwrite(VMX_GUEST_GS_LIMIT, 0xffffffff);
  vmwrite(VMX_GUEST_GS_BASE, 0);

  vmwrite(VMX_GUEST_GDTR_LIMIT, 0x3f);
  vmwrite(VMX_GUEST_GDTR_BASE, Gdt);

  vmwrite(VMX_GUEST_LDTR_SEL, 0x0);
  vmwrite(VMX_GUEST_LDTR_ACCESS_RIGHTS, 0x10000);
  vmwrite(VMX_GUEST_LDTR_LIMIT, 0);
  vmwrite(VMX_GUEST_LDTR_BASE, 0);

  vmwrite(VMX_GUEST_IDTR_LIMIT, 0x3f);
  vmwrite(VMX_GUEST_IDTR_BASE, Idt);

  vmwrite(VMX_GUEST_TR_SEL, 0x28);
  vmwrite(VMX_GUEST_TR_ACCESS_RIGHTS, 0x108b);
  vmwrite(VMX_GUEST_TR_LIMIT, 67);
  vmwrite(VMX_GUEST_TR_BASE, 0);

  vmwrite(VMX_VMCS_LINK_PTR, 0xffffffffffffffffULL);


  enum exceptions
    {
      de    = 1<<0,
      db    = 1<<1,
      bp    = 1<<3,
      gp    = 1<<13,
      pf    = 1<<14,
      ac    = 1<<17
    };

  /* In 32 bits we also want to test for interrupt handling */
  if (long_mode)
    vmwrite(VMX_EXCEPTION_BITMAP, bp|gp|pf);
  else
    vmwrite(VMX_EXCEPTION_BITMAP, gp|pf|ac);

  vmwrite(VMX_PF_ERROR_CODE_MATCH, 0);
  vmwrite(VMX_PF_ERROR_CODE_MASK, 0);

  vmwrite(VMX_ENTRY_CTRL,
          l4_vm_vmx_read(vmcb, VMX_ENTRY_CTRL) | (1 << 15)); // load efer
  vmwrite(VMX_EXIT_CTRL,
          l4_vm_vmx_read(vmcb, VMX_EXIT_CTRL) | (1 << 20)); // save guest efer
  vmwrite(VMX_EXIT_CTRL,
          l4_vm_vmx_read(vmcb, VMX_EXIT_CTRL) | (1 << 21)); // load host efer
  vmwrite(VMX_ENTRY_CTRL,
          l4_vm_vmx_read(vmcb, VMX_ENTRY_CTRL) &~ (1 << 9)); // enable long mode

  vmwrite(VMX_PRIMARY_EXEC_CTRL,
          l4_vm_vmx_read(vmcb, VMX_PRIMARY_EXEC_CTRL) &~ (1 << 23)); // do not intercept dr reads/writes
}

void
Vmx::set_rax(l4_umword_t rax)
{
  vcpu->r.ax = rax;
}

void
Vmx::set_rsp(l4_umword_t rsp)
{
  vmwrite(VMX_GUEST_RSP, rsp);
}

void
Vmx::set_rflags(l4_umword_t rflags)
{
  vmwrite(VMX_GUEST_RFLAGS, rflags);
}

void
Vmx::set_rip(l4_umword_t rip)
{
  vmwrite(VMX_GUEST_RIP, rip);
}

void
Vmx::set_cr0(l4_umword_t cr0)
{
  vmwrite(VMX_GUEST_CR0, cr0);
}

void
Vmx::set_cr3(l4_umword_t cr3)
{
  vmwrite(VMX_GUEST_CR3, cr3);
}

void
Vmx::set_cr4(l4_umword_t cr4)
{
  vmwrite(VMX_GUEST_CR4, cr4 | 0x2000); // vmx requires cr4.vmxe
  vmwrite(VMX_CR4_READ_SHADOW, cr4 | 0x2000); // let the guest see the real value
}

void
Vmx::set_dr7(l4_umword_t dr7)
{
  vmwrite(VMX_GUEST_DR7, dr7);
}

l4_umword_t
Vmx::get_rax()
{
  return vcpu->r.ax;
}

void
Vmx::enable_npt()
{}

void
Vmx::disable_npt()
{}

void
Vmx::set_efer(l4_umword_t efer)
{
  vmwrite(VMX_GUEST_IA32_EFER, efer & 0xF01);
  if (efer & 0x100) // do we want long mode?
    vmwrite(VMX_ENTRY_CTRL,
            l4_vm_vmx_read(vmcb, VMX_ENTRY_CTRL) | (1 << 9)); // enable long mode
  // Apparently vmx needs the enable long mode entry control set if the vmm
  // switches the guest to long mode.  We need to know if the entry_control
  // needs to be set if the guest switches to long mode on its own.
}

void
Vmx::jump_over_current_insn(unsigned bytes)
{
  l4_umword_t l = l4_vm_vmx_read_32(vmcb,
                                    VMX_EXIT_INSTRUCTION_LENGTH);
  if (bytes)
    l = bytes;
  l4_umword_t ip = l4_vm_vmx_read_nat(vmcb, VMX_GUEST_RIP);
  vmwrite(VMX_GUEST_RIP, ip + l);
}

unsigned
Vmx::handle_vmexit()
{
  l4_msgtag_t tag;
  l4_uint32_t interrupt_info;

  l4_uint32_t exit_reason = l4_vm_vmx_read_32(vmcb, VMX_EXIT_REASON);

  printf("# exit_code=%d rip = 0x%lx rsp = 0x%lx cs = %x ds = %x ss = %x\n",
         exit_reason, l4_vm_vmx_read_nat(vmcb, VMX_GUEST_RIP), l4_vm_vmx_read_nat(vmcb, VMX_GUEST_RSP),
         l4_vm_vmx_read_16(vmcb, VMX_GUEST_CS_SEL),
         l4_vm_vmx_read_16(vmcb, VMX_GUEST_DS_SEL),
         l4_vm_vmx_read_16(vmcb, VMX_GUEST_SS_SEL)
         );

  switch (exit_reason & 0xffff)
    {
    case 0:
      if (verbose)
        printf("# Exception or NMI at guest IP 0x%lx, checking interrupt info\n",
               l4_vm_vmx_read_nat(vmcb, VMX_GUEST_RIP));
      interrupt_info = l4_vm_vmx_read_32(vmcb, VMX_EXIT_INTERRUPT_INFO);
      // check valid bit
      if (!(interrupt_info & (1 << 31)))
        printf("# Interrupt info not valid\n");
      if (verbose)
        printf("# interrupt vector=%d, type=%d, error code valid=%d\n",
               (interrupt_info & 0xFF), ((interrupt_info & 0x700) >> 8),
               ((interrupt_info & 0x800) >> 11));
      if ((interrupt_info & 0x800) >> 11) {
        unsigned long error = l4_vm_vmx_read_32(vmcb, VMX_EXIT_INTERRUPT_ERROR);
        printf ("# error code: %lx\n", error);
        printf ("# idt vectoring info field: %x\n", l4_vm_vmx_read_32(vmcb, VMX_IDT_VECTORING_INFO_FIELD));
        printf ("# idt vectoring error code: %x\n", l4_vm_vmx_read_32(vmcb, VMX_IDT_VECTORING_ERROR));
      }

      switch ((interrupt_info & 0x700) >> 8)
        {
        case 0x6:
          printf("# Software interrupt\n");
          break;
        case 0x3:
          if (verbose)
            printf("# Hardware exception\n");
          if ((interrupt_info & 0xff) == 0x6)
            {
              printf("# undefined instruction\n");
              jump_over_current_insn(2);
              return 1;
            }
          else if ((interrupt_info & 0xff) == 0xe)
            {
              printf("# Pagefault\n");
              printf("# EFER = %llx\n", l4_vm_vmx_read_64(vmcb,
                                                        VMX_GUEST_IA32_EFER));
              return 0;
            }
          else if ((interrupt_info & 0xff) == 0x8)
            {
              printf("# Double fault. VERY BAD.\n");
              return 0;
            }
          else if ((interrupt_info & 0xff) == 17)
            {
              // this is expected
              // make the stack aligned again and restart insn
              printf("# Fetched an alignment check exception.\n");
              vmwrite(VMX_GUEST_RSP, l4_vm_vmx_read_nat(vmcb, VMX_GUEST_RSP) + 3);
              return Alignment_check_intercept;
            }
          break;
        case 0x2:
          printf("# NMI\n");
          return 0;
        default:
          printf("# Unknown\n");
          return 0;
        }

      if ((interrupt_info & (1 << 11))) // interrupt error code valid?
        if (verbose)
          printf("# interrupt error=0x%x\n",
                 l4_vm_vmx_read_32(vmcb, VMX_EXIT_INTERRUPT_ERROR));

      if (((interrupt_info & 0x700) >> 8) == 3
          && (interrupt_info & 0xff) == 14)
        {
          if (0)
            {
              l4_umword_t fault_addr = l4_vm_vmx_read_nat(vmcb,
                                                          VMX_EXIT_QUALIFICATION);
              tag = vm_cap->map(L4Re::Env::env()->task(),
                                l4_fpage(fault_addr & L4_PAGEMASK, L4_PAGESHIFT,
                                         L4_FPAGE_RW),
                                l4_map_control(fault_addr, 0, L4_MAP_ITEM_MAP));
              if (l4_error(tag))
                printf("# Error mapping page\n");
            }
          break;
        }

      jump_over_current_insn(0);
      break;
    case 1:
      printf("# External interrupt\n");
      break;
    case 18:
      if (verbose)
        printf("# vmcall ecx = %ld edx = %lx\n", vcpu->r.cx, vcpu->r.dx);
      jump_over_current_insn(0);
      return VMCALL;
#if 0
      if (vcpu->r.dx == 0x42) {
        printf("ok - #dz exception handled - interrupt handling works\n");
      }
      if (vcpu->r.dx == 0x44) {
        printf("#DB exception handled in-guest without involvement of VMM. Must not happen!\n");
        test_ok = false;
        return 0;
      }
      if (vcpu->r.dx == 0x50) {
        printf("#AC exception handled in-guest without involvement of VMM. Must not happen!\n");
        test_ok = false;
        return 0;
      }
      vcpu->r.ax = vcpu->r.cx;
#endif
      break;
    case 28:
      printf("# Control register access.\n");
      jump_over_current_insn(0);
      return 1;
    case 29:
      printf("# mov dr procbased_ctls=%llx\n", l4_vm_vmx_read(vmcb, VMX_PRIMARY_EXEC_CTRL));
      jump_over_current_insn(0);
      return 1;
    case 31:
      printf("# rdmsr\n");
      return 0;
    case 33:
      printf("# Invalid guest state.\n");
      return 0;
    case 48: // EPT violation
        {
          printf("# EPT violation\n");
          l4_umword_t q = l4_vm_vmx_read_nat(vmcb, VMX_EXIT_QUALIFICATION);
          printf("#   exit qualification: %lx\n", q);
          printf("#   guest phys = %llx,  guest linear: %lx\n",
                 l4_vm_vmx_read_64(vmcb, 0x2400), l4_vm_vmx_read_nat(vmcb, 0x640a));
          printf("#   guest cr0 = %lx\n",
                 l4_vm_vmx_read_nat(vmcb, VMX_GUEST_CR0));

          if (0)
            {
              l4_umword_t fault_addr = l4_vm_vmx_read_64(vmcb, 0x2400);
              printf("# detected pagefault @ %lx\n", fault_addr);
              tag = vm_cap->map(L4Re::Env::env()->task(),
                                l4_fpage(fault_addr & L4_PAGEMASK,
                                         L4_PAGESHIFT, L4_FPAGE_RWX),
                                l4_map_control(fault_addr, 0, L4_MAP_ITEM_MAP));
              if (l4_error(tag))
                printf("# Error mapping page\n");
            }
          return 0;
        }
      break;
    default:
      printf("# Unhandled exit reason %d\n", exit_reason);
      return 0;
    }
  return 1;
}


/*
 * Copyright (C) 2014-2015 Kernkonzept GmbH.
 * Author(s): Steffen Liebergeld <steffen.liebergeld@kernkonzept.com>
 */
/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Henning Schild <hschild@os.inf.tu-dresden.de>
 * economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include "svm.h"
#include <l4/util/cpu.h>

#include <cstdio>

static char const *const svm_features[] = {
  "Nested Paging",
  "LbrVirt",
  "SVML",
  "nRIP",
  "TscRateMsr",
  "VmcbClean",
  "FlushByAsid",
  "DecodeAssists",
  0,
  0,
  "PauseFilter",
  0,
  "PauseFilterThreshold",
  "AVIC",
};

bool
Svm::cpu_virt_capable()
{
  l4_umword_t ax, bx, cx, dx;

  if (!l4util_cpu_has_cpuid())
    return false;

  l4util_cpu_cpuid(0x80000001, &ax, &bx, &cx, &dx);

  if (!(cx & 4))
    {
      printf("# CPU does not support SVM.\n");
      return false;
    }

  l4util_cpu_cpuid(0x8000000a, &ax, &bx, &cx, &dx);

  printf("# SVM revision: %lx\n", ax & 0xf);
  printf("# Number of ASIDs: %lu\n", bx);

  printf("# This CPU has the following extra SVM features:\n");

  for (unsigned i = 0; i < (sizeof(svm_features)/sizeof(svm_features[0])); ++i)
    if (svm_features[i] && dx & (1 << i))
      printf("#\t%s\n", svm_features[i]);

  return true;
}

bool
Svm::npt_available()
{
  l4_umword_t ax, bx, cx, dx;

  if (!l4util_cpu_has_cpuid())
    return false;

  l4util_cpu_cpuid(0x8000000a, &ax, &bx, &cx, &dx);

  printf("# NPT available: %s\n", dx & 1 ? "yes" : "no");

  return (dx & 1);
}

void
Svm::initialize_vmcb(unsigned)
{
  vmcb_s->control_area.np_enable = 1;
  vmcb_s->control_area.guest_asid_tlb_ctl = 1;

  vmcb_s->state_save_area.es.selector = 0;
  vmcb_s->state_save_area.es.attrib = 0;
  vmcb_s->state_save_area.es.limit = 0;
  vmcb_s->state_save_area.es.base = 0ULL;

  //vmcb[256 +  0] = 0;        // es; attrib sel

  vmcb_s->state_save_area.cs.selector = 0x10;
  vmcb_s->state_save_area.cs.attrib = 0xc9b;
  vmcb_s->state_save_area.cs.limit = 0xffffffff;
  vmcb_s->state_save_area.cs.base = 0ULL;

  vmcb_s->state_save_area.ss.selector = 0x18;
  vmcb_s->state_save_area.ss.attrib = 0xc93;
  vmcb_s->state_save_area.ss.limit = 0xffffffff;
  vmcb_s->state_save_area.ss.base = 0ULL;

  vmcb_s->state_save_area.ds.selector = 0x20;
  vmcb_s->state_save_area.ds.attrib = 0xcf3;
  vmcb_s->state_save_area.ds.limit = 0xffffffff;
  vmcb_s->state_save_area.ds.base = 0ULL;

  vmcb_s->state_save_area.fs.selector = 0;
  vmcb_s->state_save_area.fs.attrib = 0xcf3;
  vmcb_s->state_save_area.fs.limit = 0xffffffff;
  vmcb_s->state_save_area.fs.base = 0ULL;

  vmcb_s->state_save_area.gs.selector = 0;
  vmcb_s->state_save_area.gs.attrib = 0xcf3;
  vmcb_s->state_save_area.gs.limit = 0xffffffff;
  vmcb_s->state_save_area.gs.base = 0ULL;

  vmcb_s->state_save_area.gdtr.selector = 0;
  vmcb_s->state_save_area.gdtr.attrib = 0;
  vmcb_s->state_save_area.gdtr.limit = 0xffff;
  vmcb_s->state_save_area.gdtr.base = Gdt;

  vmcb_s->state_save_area.ldtr.selector = 0;
  vmcb_s->state_save_area.ldtr.attrib = 0;
  vmcb_s->state_save_area.ldtr.limit = 0;
  vmcb_s->state_save_area.ldtr.base = 0;

  vmcb_s->state_save_area.idtr.selector = 0;
  vmcb_s->state_save_area.idtr.attrib = 0;
  vmcb_s->state_save_area.idtr.limit = 0xffff;
  vmcb_s->state_save_area.idtr.base = Idt;

  vmcb_s->state_save_area.tr.selector = 0x28;
  vmcb_s->state_save_area.tr.attrib = 0x8b;
  vmcb_s->state_save_area.tr.limit = 0x67;
  vmcb_s->state_save_area.tr.base = 0;

  vmcb_s->state_save_area.g_pat = 0x7040600010406ULL;

  vmcb_s->state_save_area.dr6 = 0;
  vmcb_s->state_save_area.cpl = 0;

  vmcb_s->control_area.intercept_exceptions = 0x0;
  vmcb_s->control_area.intercept_instruction1 |= 0x20; // intercept int1
  vmcb_s->control_area.intercept_wr_crX |= 0xff; // intercept write to cr
  vmcb_s->control_area.intercept_rd_crX |= 0xff; // intercept reads to cr

  vmcb_s->control_area.exitcode = 0;

  vmcb_s->control_area.clean_bits = 0;

}

void
Svm::set_rax(l4_umword_t rax)
{
  vmcb_s->state_save_area.rax = rax;
}

void
Svm::set_rsp(l4_umword_t rsp)
{
  vmcb_s->state_save_area.rsp = rsp;
}

void
Svm::set_rflags(l4_umword_t rflags)
{
  vmcb_s->state_save_area.rflags = rflags;
}

void
Svm::set_rip(l4_umword_t rip)
{
  vmcb_s->state_save_area.rip = rip;
}

void
Svm::set_cr0(l4_umword_t cr0)
{
  vmcb_s->state_save_area.cr0 = cr0;
}

void
Svm::set_cr3(l4_umword_t cr3)
{
  vmcb_s->state_save_area.cr3 = cr3;
}


void
Svm::set_cr4(l4_umword_t cr4)
{
  vmcb_s->state_save_area.cr4 = cr4;
}

void
Svm::set_dr7(l4_umword_t dr7)
{
  vmcb_s->state_save_area.dr7 = dr7;
}

l4_umword_t
Svm::get_rax()
{
  return vmcb_s->state_save_area.rax;
}

void
Svm::enable_npt()
{
  vmcb_s->control_area.np_enable = 1;
}

void
Svm::disable_npt()
{
  vmcb_s->control_area.np_enable = 0;
}

void
Svm::set_efer(l4_umword_t efer)
{
  vmcb_s->state_save_area.efer = efer;
}

unsigned
Svm::handle_vmexit()
{
  printf("# exit code=%llx rip=0x%llx intercept_exceptions=0x%x\n",
         vmcb_s->control_area.exitcode, vmcb_s->state_save_area.rip,
         vmcb_s->control_area.intercept_exceptions);

  switch (vmcb_s->control_area.exitcode)
    {
    case 0x4:
      printf("# Read of cr4 intercepted.\n");
      vmcb_s->state_save_area.rip += 1;
      return 1;
    case 0x14:
      printf("# Write to cr4 intercepted.\n");
      vmcb_s->state_save_area.rip += 1;
      return 1;
    case 0x41:
      printf("# divide by zero exception encountered\n");
      vmcb_s->state_save_area.rip += 1;
      return 1;
    case 0x43:
      printf("# Software interrupt\n");
      vmcb_s->state_save_area.rip += 1;
      return 1;
    case 0x46:
      printf("# undefined instruction\n");
      vmcb_s->state_save_area.rip += 2;
      return 1;
    case 0x4d:
      printf("# General protection fault. Bye\n");
      printf("# cs=%08x attrib=%x, limit=%x, base=%llx\n",
              vmcb_s->state_save_area.cs.selector,
              vmcb_s->state_save_area.cs.attrib,
              vmcb_s->state_save_area.cs.limit,
              vmcb_s->state_save_area.cs.base);
      printf("# ss=%08x attrib=%x, limit=%x, base=%llx\n",
              vmcb_s->state_save_area.ss.selector,
              vmcb_s->state_save_area.ss.attrib,
              vmcb_s->state_save_area.ss.limit,
              vmcb_s->state_save_area.ss.base);
      printf("# np_enabled=%lld\n", vmcb_s->control_area.np_enable);
      printf("# cr0=%llx cr4=%llx\n", vmcb_s->state_save_area.cr0,
              vmcb_s->state_save_area.cr4);
      printf("# interrupt_ctl=%llx\n", vmcb_s->control_area.interrupt_ctl);
      printf("# rip=%llx, rsp=%llx, cpl=%d\n", vmcb_s->state_save_area.rip,
              vmcb_s->state_save_area.rsp, vmcb_s->state_save_area.cpl);
      printf("# exitinfo1=%llx\n", vmcb_s->control_area.exitinfo1);
      return 0;
    case 0x4e:
      printf("# page fault; error code=%llx, pfa=%llx\n",
             vmcb_s->control_area.exitinfo1, vmcb_s->control_area.exitinfo2);
      return 0;
    case 0x51:
      printf("# alignment check exception encountered.\n");
      vmcb_s->state_save_area.rsp += 3;
      return Alignment_check_intercept;
    case 0x7f:
      printf("# shutdown.\n");
      return 0;
    case 0x400:
      printf("# host-level page fault; error code=%llx, gpa=%llx\n",
             vmcb_s->control_area.exitinfo1, vmcb_s->control_area.exitinfo2);
      return 0;
    case 0x81:
      printf("# VMMCALL ecx = %ld edx=%lx\n", (long)(vcpu->r.cx & 0xffffffff), vcpu->r.dx);
      vmcb_s->state_save_area.rip += 3;
      return VMCALL;
    case l4_uint64_t(~0):
      printf("# Invalid guest state.\n");
      return 0;
    default:
      printf("# Unhandled vm exit.\n");
      return 0;
    }
  return 1;
}


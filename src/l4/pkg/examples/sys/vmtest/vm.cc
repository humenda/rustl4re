/*
 * Copyright (C) 2014-2015 Kernkonzept GmbH.
 * Author(s): Steffen Liebergeld <steffen.liebergeld@kernkonzept.com>
 *
 * This file is distributed under the terms of the GNU General Public
 * License, version 2.  Please see the COPYING-GPL-2 file for details.
 */
#include "vm.h"

#include <l4/util/util.h>
#include <l4/re/env>
#include <l4/re/util/cap_alloc>
#include <l4/sys/factory>
#include <l4/sys/thread>
#include <l4/vcpu/vcpu.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

// code to be run inside virtual machine
extern char guestcode32[], guestcode64[];
extern char interrupt_handlers[]; // implemented in guest.S
extern char irq_dz[], irq_db[], irq_bp[], irq_ac[];

// valid in 32bit only
class Idt_entry
{
private:
  l4_uint16_t _offset_low;
  l4_uint16_t _segment_selector;
  l4_uint8_t  _ist;
  l4_uint8_t  _access;
  l4_uint16_t _offset_high;
public:
  Idt_entry() {}
  void set(char *isr, l4_uint16_t selector);
} __attribute__((packed));

void
Idt_entry::set(char *isr, l4_uint16_t selector)
{
  // calculate isr guest address
  isr = isr - interrupt_handlers + (char*)Interrupt_handler;
  _offset_low = (unsigned long)isr & 0x0000ffff;
  _segment_selector  = selector;
  _ist        = 0;
  _access     = 0x8f;
  _offset_high= ((unsigned long)isr & 0xffff0000)>>16;
}


class Gdt_entry
{
private:
  unsigned short _segment_limit_low;
  unsigned short _base_address_low;
  unsigned char  _base_address_middle;
  unsigned short _flags;
  unsigned char  _base_address_upper;
public:
  enum{
    Shift_segment_type = 0,
    Shift_descriptor_type = 4,
    Shift_dpl = 5,
    Shift_present = 7,
    Shift_segment_limit = 8,
    Shift_avl = 12,
    Shift_long = 13,
    Shift_size = 14,
    Shift_granularity = 15,
  };
  enum Segment_type{ // relative to _flags
    Data_ro= 0x0,
    Data_roa= 0x1,
    Data_rw= 0x2,
    Data_rwa=0x3,
    Data_ro_expand_down=0x4,
    Data_roa_expand_down=0x5,
    Data_rw_expand_down=0x6,
    Data_rwa_expand_down=0x7,
    Code_xo = 0x8,
    Code_xoa = 0x9,
    Code_xr = 0xa,
    Code_xra = 0xb,
    Code_xo_conforming = 0xc,
    Code_xo_conforming_a = 0xd,
    Code_xr_conforming = 0xe,
    Code_xr_conforming_a = 0xf,
  };
  enum Dpl{
    User   = 0x3,
    System = 0x0
  };
  Gdt_entry() {}
  void set(
      unsigned long base_address,
      unsigned long segment_limit,
      Dpl dpl,
      Segment_type type);
  void set_flat(Dpl dpl, Segment_type type)
  {
    set(0, 0xffffffff, dpl, type);
  }
  void set_null()
  {
    set_flat(Gdt_entry::System, Gdt_entry::Data_ro);
  }
} __attribute__((packed));

void
Gdt_entry::set(
    unsigned long base_address,
    unsigned long segment_limit,
    Dpl dpl,
    Segment_type type)
{
  if (segment_limit)
    _flags |= 1 << Shift_present;
  _segment_limit_low = 0x0000ffff & segment_limit; 
  _flags |= ((0x000f0000 & segment_limit) >> 16) << Shift_segment_limit;
  _base_address_low = 0x0000ffff & base_address;
  _base_address_middle = (0x00ff0000 & base_address) >> 16;
  _base_address_upper = (0xff000000 & base_address) >> 24;
  _flags |= type;
  _flags |= dpl << Shift_dpl;
  if (dpl != User)
    _flags |= 1 << Shift_avl;
  _flags |= 1 << Shift_size; // 32bit
  _flags |= 0 << Shift_long;
  _flags |= 1 << Shift_granularity;
  _flags |= 1 << Shift_descriptor_type;
}


static Idt_entry idt[32] __attribute__((aligned(4096)));
static Gdt_entry gdt[32] __attribute__((aligned(4096)));
static char guest_stack[STACK_SIZE] __attribute__((aligned(4096)));
static char handler_stack[STACK_SIZE];

l4_uint64_t pde __attribute__((aligned(4096)));
l4_uint64_t pdpte __attribute__((aligned(4096)));
l4_uint64_t pml4e __attribute__((aligned(4096)));

void
Vm::setup_vm()
{
  l4_addr_t ext_state;
  l4_msgtag_t msg;
  int ret;

  printf("# Checking if CPU is capable of virtualization: ");
  if (!cpu_virt_capable())
    {
      printf("CPU does not support virtualization. Bye.\n");
      exit(1);
    }
  printf("# It is.\n");

  printf("# Allocating a VM capability: ");
  vm_cap = L4Re::Util::cap_alloc.alloc<L4::Vm>();
  if (!vm_cap.is_valid())
    {
      printf("Failure.\n");
      exit(1);
    }
  printf("Success.\n");

  printf("# Creating a VM kernel object: ");
  msg = L4Re::Env::env()->factory()->create(vm_cap);
  if (l4_error(msg))
    {
      printf("Failure.\n");
      exit(1);
    }
  printf("Success.\n");

  printf("# Trying to allocate vCPU extended state: ");
  ret = l4vcpu_ext_alloc(&vcpu, &ext_state, L4_BASE_TASK_CAP,
                         L4Re::Env::env()->rm().cap());
  if (ret)
    {
      printf("Could not find vCPU extended state mem: %d\n", ret);
      exit(1);
    }
  printf("Success.\n");

  vmcb = (void*)ext_state;
  if (vmcb == 0)
    {
      printf("# vCPU extended state @NULL. Exit.\n");
      exit (1);
    }

  vcpu->state = L4_VCPU_F_FPU_ENABLED;
  vcpu->saved_state = L4_VCPU_F_USER_MODE
                      | L4_VCPU_F_FPU_ENABLED
                      | L4_VCPU_F_IRQ;

  vcpu->entry_ip = (l4_umword_t)&handler;
  vcpu->entry_sp = (l4_umword_t)(handler_stack + STACK_SIZE);
  vcpu->user_task = vm_cap.cap();


  printf("# Trying to switch vCPU to extended operation: ");
  msg = l4_thread_vcpu_control_ext(L4_INVALID_CAP, (l4_addr_t)vcpu);
  ret = l4_error(msg); if (ret)
    {
      printf("Could not enable ext vCPU: %d\n", ret);
      exit(1);
    }
  printf("Success.\n");

  printf("# Clearing guest stack.\n");
  memset(guest_stack, 0, sizeof(guest_stack));
  printf("# Clearing handler stack.\n");
  memset(handler_stack, 0, sizeof(handler_stack));
  printf("# done.\n");

  l4_touch_ro((void*)guestcode32, 2);
  printf("# Mapping code from %p to %p: ", (void*)guestcode32, (void*)Code);
  msg = vm_cap->map(L4Re::Env::env()->task(),
                    l4_fpage((l4_umword_t)guestcode32 & L4_PAGEMASK, L4_PAGESHIFT,
                             L4_FPAGE_RWX),
                    l4_map_control((l4_umword_t)Code, 0, L4_MAP_ITEM_MAP));
  if ((ret = l4_error(msg)))
    {
      printf("failure: %d\n", ret);
      exit(1);
    }
  else
    printf("success\n");

  l4_touch_ro((void*)interrupt_handlers, 2);
  printf("# Mapping interrupt handler from %p to %p: ", (void*)interrupt_handlers, (void*)Interrupt_handler);
  msg = vm_cap->map(L4Re::Env::env()->task(),
                    l4_fpage((l4_umword_t)interrupt_handlers & L4_PAGEMASK, L4_PAGESHIFT,
                             L4_FPAGE_RWX),
                    l4_map_control((l4_umword_t)Interrupt_handler, 0, L4_MAP_ITEM_MAP));
  if ((ret = l4_error(msg)))
    {
      printf("failure: %d\n", ret);
      exit(1);
    }
  else
    printf("success\n");

  l4_touch_ro(&is_vmx, 2);
  printf("# Mapping flags from %p to %p: ", &is_vmx, (void*)Flags);
  msg = vm_cap->map(L4Re::Env::env()->task(),
                    l4_fpage((l4_umword_t)&is_vmx & L4_PAGEMASK, L4_PAGESHIFT,
                             L4_FPAGE_RX),
                    l4_map_control((l4_umword_t)Flags, 0, L4_MAP_ITEM_MAP));
  if ((ret = l4_error(msg)))
    {
      printf("failure: %d\n", ret);
      exit(1);
    }
  else
    printf("success\n");

  printf("# Mapping stack: \n");
  for (l4_umword_t ofs = 0; ofs < STACK_SIZE; ofs += L4_PAGESIZE)
    {
      printf("# %p -> %p\n", (void*)((l4_umword_t)guest_stack + ofs), (void*)((l4_umword_t)Stack +  ofs));
      msg = vm_cap->map(L4Re::Env::env()->task(),
                        l4_fpage(((l4_umword_t)(guest_stack) + ofs) & L4_PAGEMASK,
                                 L4_PAGESHIFT, L4_FPAGE_RWX),
                        l4_map_control(Stack +  ofs, 0,
                                       L4_MAP_ITEM_MAP));
    }
  if ((ret = l4_error(msg)))
    {
      printf("failure: %d\n", ret);
      exit(1);
    }
  else
    printf("success\n");

  idt[ 0].set(irq_dz, 0x8); // #DZ divide by zero
  idt[ 1].set(irq_db, 0x8); // #DB debug exception
  idt[ 3].set(irq_bp, 0x8); // #BP breakpoint (int3)
  idt[17].set(irq_ac, 0x8); // #AC alignment check

  gdt[0].set_null();
  gdt[1].set_flat(Gdt_entry::System, Gdt_entry::Code_xra); // 0x8
  gdt[2].set_flat(Gdt_entry::System, Gdt_entry::Data_rwa); // 0x10
  gdt[3].set_flat(Gdt_entry::User,   Gdt_entry::Code_xra); // 0x18
  gdt[4].set_flat(Gdt_entry::User,   Gdt_entry::Data_rwa); // 0x20
  gdt[5].set_flat(Gdt_entry::System, Gdt_entry::Data_rwa); // 0x28
  gdt[6].set_flat(Gdt_entry::User,   Gdt_entry::Data_rwa); // 0x30

  printf("# Mapping idt from %p to %p: ", (void*)idt, (void*)Idt);
  msg = vm_cap->map(L4Re::Env::env()->task(),
                    l4_fpage((l4_addr_t)idt & L4_PAGEMASK, L4_PAGESHIFT,
                             L4_FPAGE_RW),
                    l4_map_control((l4_addr_t)Idt, 0, L4_MAP_ITEM_MAP));
  if ((ret = l4_error(msg)))
    {
      printf("failure: %d\n", ret);
      exit(1);
    }
  else
    printf("success\n");

  printf("# Mapping gdt from %p to %p: ", (void*)gdt, (void*)Gdt);
  msg = vm_cap->map(L4Re::Env::env()->task(),
                    l4_fpage((l4_addr_t)gdt & L4_PAGEMASK, L4_PAGESHIFT,
                             L4_FPAGE_RW),
                    l4_map_control((l4_addr_t)Gdt, 0, L4_MAP_ITEM_MAP));
  if ((ret = l4_error(msg)))
    {
      printf("failure: %d\n", ret);
      exit(1);
    }
  else
    printf("success\n");

}

unsigned
Vm::vm_resume()
{
  int ret;
  l4_msgtag_t msg;

  msg = l4_thread_vcpu_resume_commit(L4_INVALID_CAP,
                                     l4_thread_vcpu_resume_start());

  ret = l4_error(msg);
  if (ret)
    {
      printf("# vm_resume failed: %s (%d)\n", l4sys_errtostr(ret), ret);
      return ret;
    }

  return ret;
}

void
Vm::run_tests()
{
  int ret;
  l4_umword_t rflags;

  initialize_vmcb(0);

  set_rip(Code);
  set_cr3(0);
  set_dr7(0xffffffff);
  set_efer(0x1000);

  // now configure CPU
#if 0
  if (npt)
    enable_npt();
  else
    disable_npt();
#endif

  asm volatile("pushf     \n"
               "pop %0   \n"
               : "=r" (rflags));

  // clear interrupt flag
  rflags &= 0xfffffdff;
  // add AC (alignment check) flag
  rflags |= (1 << 18);

  l4_uint32_t cr0 = 0x1003b;
  // add AM flag
  cr0 |= (1 << 18);

  set_rsp((l4_umword_t)Stack + STACK_SIZE - 1);
  set_rflags(rflags);
  set_cr0(cr0);
  set_cr4(0x6b0);
  set_dr7(0xffffffff);
  vcpu->r.dx = vcpu->r.cx = vcpu->r.bx = vcpu->r.bp = vcpu->r.si = vcpu->r.di = 0;
  set_rax(0);

  // switch to VM
  vm_resume();
  ret = handle_vmexit();
  if (ret == VMCALL)
    {
      if (vcpu->r.dx == 0xdeadbeef)
        printf("ok - vm running\n");
    }
  else
    printf("not ok - vm not running\n");

  vm_resume();
  ret = handle_vmexit();
  if (ret == VMCALL)
    {
      if (vcpu->r.dx == 0x42)
        printf("ok - divide by zero exception handled correctly by the guest\n");
      else
        printf("not ok - wrong interrupt handler for #dz\n");
    }
  else if (ret == Exception_intercept) // svm intercepts #dz even when configured to not intercept
    printf("ok - #dz intercepted\n");
  else
    printf("not ok - #dz not handled correctly\n");

  vm_resume();
  ret = handle_vmexit();
  if (ret == Alignment_check_intercept)
    printf("ok - alignment check exception intercepted\n");
  else
    printf("not ok - alignment check exception not intercepted\n");

#if __x86_64__
  // test long mode if host on ia32e host
  set_efer(0x1500);
  set_dr7(0x700);
  set_cr4(0x6b0);
  set_cr3(make_ia32e_cr3());
  set_cr0(0x8001003b);
  set_rip((guestcode64 - guestcode32) + Code);
  vm_resume();
  ret = handle_vmexit();
  if (ret == VMCALL && vcpu->r.dx == 0x4242)
    printf("ok - long mode guest works\n");
  else
    printf("not ok - long mode guest not working\n");
#endif
}

void
Vm::handler()
{
  printf("Received an IRQ. Considering this as ERROR.\n");
  exit(1);
}

/**
 * Create a very primitive PAE pagetable that establishes an identity mapping
 * of the first 2MB of guest address space.
 *
 * \returns The cr3 to use when running the guest in ia32e mode.
 */
l4_umword_t
Vm::make_ia32e_cr3()
{
  l4_umword_t cr3;
  l4_msgtag_t tag;
  int ret;

  cr3 = (unsigned long)&pml4e;

  // prepare pml4e
  pml4e = (unsigned long)&pdpte
          | 0x4 /*user allowed*/
          | 0x2 /*write allowed*/
          | 0x1 /*present*/;
  // prepare pdpte
  pdpte = (unsigned long)&pde
          | 0x4 /*user allowed*/
          | 0x2 /*write enabled*/
          | 0x1 /*present*/;
  // prepare pte
  pde = 0x0
          | (1 << 7) /*2mb page*/
          | 0x4 /*user allowed*/
          | 0x2 /*write enabled*/
          | 0x1 /*present*/;

  if ((l4_umword_t)&pml4e < 0x80000000)
    printf("# Page tables reside below 4G.\n");

  // map pml4e
  printf("# Mapping pml4e from %p to %p ", &pml4e, &pml4e);
  tag = vm_cap->map(L4Re::Env::env()->task(),
                    l4_fpage((l4_umword_t)&pml4e,
                             L4_PAGESHIFT, L4_FPAGE_RW),
                        l4_map_control((l4_umword_t)&pml4e, 0,
                                       L4_MAP_ITEM_MAP));
  if ((ret = l4_error(tag)))
    {
      printf("error %d\n", ret);
      return ~0;
    }
  else
    printf("success\n");

  // map pdpte
  printf("# Mapping pdpte from %p to %p ", &pdpte, &pdpte);
  tag = vm_cap->map(L4Re::Env::env()->task(),
                    l4_fpage((l4_umword_t)&pdpte,
                             L4_PAGESHIFT, L4_FPAGE_RW),
                        l4_map_control((l4_umword_t)&pdpte, 0,
                                       L4_MAP_ITEM_MAP));

  if ((ret = l4_error(tag)))
    {
      printf("error %d\n", ret);
      return ~0;
    }
  else
    printf("success\n");

  // map pde
  printf("# Mapping pde from %p to %p ", &pde, &pde);
  tag = vm_cap->map(L4Re::Env::env()->task(),
                    l4_fpage((l4_umword_t)&pde,
                             L4_PAGESHIFT, L4_FPAGE_RW),
                        l4_map_control((l4_umword_t)&pde, 0,
                                       L4_MAP_ITEM_MAP));

  if ((ret = l4_error(tag)))
    {
      printf("error %d.\n", ret);
      return ~0;
    }
  else
    printf("success\n");

  return cr3;
}


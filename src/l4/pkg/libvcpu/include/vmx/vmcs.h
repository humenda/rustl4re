/*
 * Copyright (C) 2017 Kernkonzept GmbH.
 * Author(s): Philipp Eppelt <philipp.eppelt@kernkonzept.com>
 *
 * This file is distributed under the terms of the GNU General Public
 * License, version 2.  Please see the COPYING-GPL-2 file for details.
 *
 * As a special exception, you may use this file as part of a free software
 * library without restriction.  Specifically, if other files instantiate
 * templates or use macros or inline functions from this file, or you compile
 * this file and link it with other files to produce an executable, this
 * file does not by itself cause the resulting executable to be covered by
 * the GNU General Public License.  This exception does not however
 * invalidate any other reasons why the executable file might be covered by
 * the GNU General Public License.
 */
#pragma once

/*
 * The constants are defined in the Intel Software Developer Manual Volume 3,
 * Appendix B.
 */

/**
 * 16bit width VMCS fields
 */
enum L4vcpu_vmx_vmcs_16bit_fields
{
  L4VCPU_VMCS_VPID                         = 0x0000,
  L4VCPU_VMCS_PIR_NOTIFICATION_VECTOR      = 0x0002,
  L4VCPU_VMCS_EPTP_INDEX                   = 0x0004,

  L4VCPU_VMCS_GUEST_ES_SELECTOR            = 0x0800,
  L4VCPU_VMCS_GUEST_CS_SELECTOR            = 0x0802,
  L4VCPU_VMCS_GUEST_SS_SELECTOR            = 0x0804,
  L4VCPU_VMCS_GUEST_DS_SELECTOR            = 0x0806,
  L4VCPU_VMCS_GUEST_FS_SELECTOR            = 0x0808,
  L4VCPU_VMCS_GUEST_GS_SELECTOR            = 0x080a,
  L4VCPU_VMCS_GUEST_LDTR_SELECTOR          = 0x080c,
  L4VCPU_VMCS_GUEST_TR_SELECTOR            = 0x080e,
  L4VCPU_VMCS_GUEST_INTERRUPT_STATUS       = 0x0810,

  L4VCPU_VMCS_HOST_ES_SELECTOR             = 0x0c00,
  L4VCPU_VMCS_HOST_CS_SELECTOR             = 0x0c02,
  L4VCPU_VMCS_HOST_SS_SELECTOR             = 0x0c04,
  L4VCPU_VMCS_HOST_DS_SELECTOR             = 0x0c06,
  L4VCPU_VMCS_HOST_FS_SELECTOR             = 0x0c08,
  L4VCPU_VMCS_HOST_GS_SELECTOR             = 0x0c0a,
  L4VCPU_VMCS_HOST_TR_SELECTOR             = 0x0c0c,
};

/**
 * 32bit width VMCS fields
 */
enum L4vcpu_vmx_vmcs_32bit_fields
{
  L4VCPU_VMCS_PIN_BASED_VM_EXEC_CTLS       = 0x4000,
  L4VCPU_VMCS_PRI_PROC_BASED_VM_EXEC_CTLS  = 0x4002,
  L4VCPU_VMCS_EXCEPTION_BITMAP             = 0x4004,
  L4VCPU_VMCS_PAGE_FAULT_ERROR_MASK        = 0x4006,
  L4VCPU_VMCS_PAGE_FAULT_ERROR_MATCH       = 0x4008,
  L4VCPU_VMCS_CR3_TARGET_COUNT             = 0x400a,

  L4VCPU_VMCS_VM_EXIT_CTLS                 = 0x400c,
  L4VCPU_VMCS_VM_EXIT_MSR_STORE_COUNT      = 0x400e,
  L4VCPU_VMCS_VM_EXIT_MSR_LOAD_COUNT       = 0x4010,

  L4VCPU_VMCS_VM_ENTRY_CTLS                = 0x4012,
  L4VCPU_VMCS_VM_ENTRY_MSR_LOAD_COUNT      = 0x4014,
  L4VCPU_VMCS_VM_ENTRY_INTERRUPT_INFO      = 0x4016,
  L4VCPU_VMCS_VM_ENTRY_EXCEPTION_ERROR     = 0x4018,
  L4VCPU_VMCS_VM_ENTRY_INSN_LEN            = 0x401a,

  L4VCPU_VMCS_TPR_THRESHOLD                = 0x401c,
  L4VCPU_VMCS_SEC_PROC_BASED_VM_EXEC_CTLS  = 0x401e,
  L4VCPU_VMCS_PLE_GAP                      = 0x4020,
  L4VCPU_VMCS_PLE_WINDOW                   = 0x4022,

  L4VCPU_VMCS_VM_INSN_ERROR                = 0x4400,
  L4VCPU_VMCS_EXIT_REASON                  = 0x4402,
  L4VCPU_VMCS_VM_EXIT_INTERRUPT_INFO       = 0x4404,
  L4VCPU_VMCS_VM_EXIT_INTERRUPT_ERROR      = 0x4406,
  L4VCPU_VMCS_IDT_VECTORING_INFO           = 0x4408,
  L4VCPU_VMCS_IDT_VECTORING_ERROR          = 0x440a,
  L4VCPU_VMCS_VM_EXIT_INSN_LENGTH          = 0x440c,
  L4VCPU_VMCS_VM_EXIT_INSN_INFO            = 0x440e,

  L4VCPU_VMCS_GUEST_ES_LIMIT               = 0x4800,
  L4VCPU_VMCS_GUEST_CS_LIMIT               = 0x4802,
  L4VCPU_VMCS_GUEST_SS_LIMIT               = 0x4804,
  L4VCPU_VMCS_GUEST_DS_LIMIT               = 0x4806,
  L4VCPU_VMCS_GUEST_FS_LIMIT               = 0x4808,
  L4VCPU_VMCS_GUEST_GS_LIMIT               = 0x480a,
  L4VCPU_VMCS_GUEST_LDTR_LIMIT             = 0x480c,
  L4VCPU_VMCS_GUEST_TR_LIMIT               = 0x480e,
  L4VCPU_VMCS_GUEST_GDTR_LIMIT             = 0x4810,
  L4VCPU_VMCS_GUEST_IDTR_LIMIT             = 0x4812,

  L4VCPU_VMCS_GUEST_ES_ACCESS_RIGHTS       = 0x4814,
  L4VCPU_VMCS_GUEST_CS_ACCESS_RIGHTS       = 0x4816,
  L4VCPU_VMCS_GUEST_SS_ACCESS_RIGHTS       = 0x4818,
  L4VCPU_VMCS_GUEST_DS_ACCESS_RIGHTS       = 0x481a,
  L4VCPU_VMCS_GUEST_FS_ACCESS_RIGHTS       = 0x481c,
  L4VCPU_VMCS_GUEST_GS_ACCESS_RIGHTS       = 0x481e,
  L4VCPU_VMCS_GUEST_LDTR_ACCESS_RIGHTS     = 0x4820,
  L4VCPU_VMCS_GUEST_TR_ACCESS_RIGHTS       = 0x4822,

  L4VCPU_VMCS_GUEST_INTERRUPTIBILITY_STATE = 0x4824,
  L4VCPU_VMCS_GUEST_ACTIVITY_STATE         = 0x4826,
  L4VCPU_VMCS_GUEST_SMBASE                 = 0x4828,
  L4VCPU_VMCS_GUEST_IA32_SYSENTER_CS       = 0x482a,
  L4VCPU_VMCS_PREEMPTION_TIMER_VALUE       = 0x482e,

  L4VCPU_VMCS_HOST_IA32_SYSENTER_CS        = 0x4c00,
};

/**
 * natural width VMCS fields
 */
enum L4vcpu_vmx_vmcs_natural_fields
{
  L4VCPU_VMCS_CR0_GUEST_HOST_MASK          = 0x6000,
  L4VCPU_VMCS_CR4_GUEST_HOST_MASK          = 0x6002,
  L4VCPU_VMCS_CR0_READ_SHADOW              = 0x6004,
  L4VCPU_VMCS_CR4_READ_SHADOW              = 0x6006,
  L4VCPU_VMCS_CR3_TARGET_VALUE0            = 0x6008,
  L4VCPU_VMCS_CR3_TARGET_VALUE1            = 0x600a,
  L4VCPU_VMCS_CR3_TARGET_VALUE2            = 0x600c,
  L4VCPU_VMCS_CR3_TARGET_VALUE3            = 0x600e,

  L4VCPU_VMCS_EXIT_QUALIFICATION           = 0x6400,
  L4VCPU_VMCS_IO_RCX                       = 0x6402,
  L4VCPU_VMCS_IO_RSI                       = 0x6404,
  L4VCPU_VMCS_IO_RDI                       = 0x6406,
  L4VCPU_VMCS_IO_RIP                       = 0x6408,
  L4VCPU_VMCS_GUEST_LINEAR_ADDRESS         = 0x640a,

  L4VCPU_VMCS_GUEST_CR0                    = 0x6800,
  L4VCPU_VMCS_GUEST_CR3                    = 0x6802,
  L4VCPU_VMCS_GUEST_CR4                    = 0x6804,
  L4VCPU_VMCS_GUEST_ES_BASE                = 0x6806,
  L4VCPU_VMCS_GUEST_CS_BASE                = 0x6808,
  L4VCPU_VMCS_GUEST_SS_BASE                = 0x680a,
  L4VCPU_VMCS_GUEST_DS_BASE                = 0x680c,
  L4VCPU_VMCS_GUEST_FS_BASE                = 0x680e,
  L4VCPU_VMCS_GUEST_GS_BASE                = 0x6810,
  L4VCPU_VMCS_GUEST_LDTR_BASE              = 0x6812,
  L4VCPU_VMCS_GUEST_TR_BASE                = 0x6814,
  L4VCPU_VMCS_GUEST_GDTR_BASE              = 0x6816,
  L4VCPU_VMCS_GUEST_IDTR_BASE              = 0x6818,
  L4VCPU_VMCS_GUEST_DR7                    = 0x681a,
  L4VCPU_VMCS_GUEST_RSP                    = 0x681c,
  L4VCPU_VMCS_GUEST_RIP                    = 0x681e,
  L4VCPU_VMCS_GUEST_RFLAGS                 = 0x6820,
  L4VCPU_VMCS_GUEST_PENDING_DBG_EXCEPTIONS = 0x6822,
  L4VCPU_VMCS_GUEST_IA32_SYSENTER_ESP      = 0x6824,
  L4VCPU_VMCS_GUEST_IA32_SYSENTER_EIP      = 0x6826,

  L4VCPU_VMCS_HOST_CR0                     = 0x6c00,
  L4VCPU_VMCS_HOST_CR3                     = 0x6c02,
  L4VCPU_VMCS_HOST_CR4                     = 0x6c04,
  L4VCPU_VMCS_HOST_FS_BASE                 = 0x6c06,
  L4VCPU_VMCS_HOST_GS_BASE                 = 0x6c08,
  L4VCPU_VMCS_HOST_TR_BASE                 = 0x6c0a,
  L4VCPU_VMCS_HOST_GDTR_BASE               = 0x6c0c,
  L4VCPU_VMCS_HOST_IDTR_BASE               = 0x6c0e,
  L4VCPU_VMCS_HOST_IA32_SYSENTER_ESP       = 0x6c10,
  L4VCPU_VMCS_HOST_IA32_SYSENTER_EIP       = 0x6c12,
  L4VCPU_VMCS_HOST_RSP                     = 0x6c14,
  L4VCPU_VMCS_HOST_RIP                     = 0x6c16,
};

/**
 * 64bit width VMCS fields
 */
enum L4vcpu_vmx_vmcs_64bit_fields
{
  L4VCPU_VMCS_ADDRESS_IO_BITMAP_A          = 0x2000,
  L4VCPU_VMCS_ADDRESS_IO_BITMAP_B          = 0x2002,
  L4VCPU_VMCS_ADDRESS_MSR_BITMAP           = 0x2004,
  L4VCPU_VMCS_VM_EXIT_MSR_STORE_ADDRESS    = 0x2006,
  L4VCPU_VMCS_VM_EXIT_MSR_LOAD_ADDRESS     = 0x2008,
  L4VCPU_VMCS_VM_ENTRY_MSR_LOAD_ADDRESS    = 0x200a,
  L4VCPU_VMCS_EXECUTIVE_VMCS_POINTER       = 0x200c,
  L4VCPU_VMCS_TSC_OFFSET                   = 0x2010,
  L4VCPU_VMCS_VIRTUAL_APIC_ADDRESS         = 0x2012,
  L4VCPU_VMCS_APIC_ACCESS_ADDRESS          = 0x2014,
  L4VCPU_VMCS_PIR_DESCRIPTOR               = 0x2016,
  L4VCPU_VMCS_VM_FUNCTION_CONTROL          = 0x2018,
  L4VCPU_VMCS_EPT_POINTER                  = 0x201a,
  L4VCPU_VMCS_EOI_EXIT_BITMAP0             = 0x201c,
  L4VCPU_VMCS_EOI_EXIT_BITMAP1             = 0x201e,
  L4VCPU_VMCS_EOI_EXIT_BITMAP2             = 0x2020,
  L4VCPU_VMCS_EOI_EXIT_BITMAP3             = 0x2022,
  L4VCPU_VMCS_EPTP_LIST_ADDRESS            = 0x2024,
  L4VCPU_VMCS_VMREAD_BITMAP_ADDRESS        = 0x2026,
  L4VCPU_VMCS_VMWRITE_BITMAP_ADDRESS       = 0x2028,
  L4VCPU_VMCS_VIRT_EXCP_INFO_ADDRESS       = 0x202a,
  L4VCPU_VMCS_XSS_EXITING_BITMAP           = 0x202c,

  L4VCPU_VMCS_GUEST_PHYSICAL_ADDRESS       = 0x2400,

  L4VCPU_VMCS_LINK_POINTER                 = 0x2800,
  L4VCPU_VMCS_GUEST_IA32_DEBUGCTL          = 0x2802,
  L4VCPU_VMCS_GUEST_IA32_PAT               = 0x2804,
  L4VCPU_VMCS_GUEST_IA32_EFER              = 0x2806,
  L4VCPU_VMCS_GUEST_IA32_PERF_GLOBAL_CTRL  = 0x2808,
  L4VCPU_VMCS_GUEST_PDPTE0                 = 0x280a,
  L4VCPU_VMCS_GUEST_PDPTE1                 = 0x280c,
  L4VCPU_VMCS_GUEST_PDPTE2                 = 0x280e,
  L4VCPU_VMCS_GUEST_PDPTE3                 = 0x2810,

  L4VCPU_VMCS_HOST_IA32_PAT                = 0x2c00,
  L4VCPU_VMCS_HOST_IA32_EFER               = 0x2c02,
  L4VCPU_VMCS_HOST_IA32_PERF_GLOBAL_CTRL   = 0x2c04,
};

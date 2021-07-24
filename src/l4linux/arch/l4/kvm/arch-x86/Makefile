# SPDX-License-Identifier: GPL-2.0

b := ../../../x86/kvm

ccflags-y += -I $(srctree)/arch/x86/kvm
ccflags-$(CONFIG_KVM_WERROR) += -Werror

CFLAGS_kvm_main.o := -I$(srctree)/virt/kvm

ifeq ($(CONFIG_FRAME_POINTER),y)
OBJECT_FILES_NON_STANDARD_vmenter.o := y
endif

KVM := ../../../../virt/kvm

kvm-y			+= kvm_main.o $(KVM)/coalesced_mmio.o \
				$(KVM)/eventfd.o $(KVM)/irqchip.o $(KVM)/vfio.o \
				$(KVM)/dirty_ring.o
kvm-$(CONFIG_KVM_ASYNC_PF)	+= $(KVM)/async_pf.o

kvm-y			+= x86.o $(b)/emulate.o $(b)/i8259.o $(b)/irq.o $(b)/lapic.o \
			   $(b)/i8254.o $(b)/ioapic.o $(b)/irq_comm.o $(b)/cpuid.o $(b)/pmu.o $(b)/mtrr.o \
			   $(b)/hyperv.o $(b)/debugfs.o mmu/mmu.o $(b)/mmu/page_track.o \
			   $(b)/mmu/spte.o
kvm-$(CONFIG_X86_64) += $(b)/mmu/tdp_iter.o $(b)/mmu/tdp_mmu.o
kvm-$(CONFIG_KVM_XEN)	+= $(b)/xen.o

kvm-y			+= kvm-l4.o
kvm-intel-y		+= vmx/vmx.o $(b)/vmx/vmenter.o $(b)/vmx/pmu_intel.o $(b)/vmx/vmcs12.o \
			   $(b)/vmx/evmcs.o vmx/nested.o $(b)/vmx/posted_intr.o
kvm-intel-$(CONFIG_X86_SGX_KVM)	+= $(b)/vmx/sgx.o

kvm-amd-y		+= svm/svm.o $(b)/svm/vmenter.o $(b)/svm/pmu.o $(b)/svm/nested.o svm/avic.o $(b)/svm/sev.o

obj-$(CONFIG_KVM)	+= kvm.o
obj-$(CONFIG_KVM_INTEL)	+= kvm-intel.o
obj-$(CONFIG_KVM_AMD)	+= kvm-amd.o

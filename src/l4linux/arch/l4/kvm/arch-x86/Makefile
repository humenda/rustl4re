# SPDX-License-Identifier: GPL-2.0

b := ../../../x86/kvm

KBUILD_CPPFLAGS := -I$(srctree)/arch/x86/kvm $(KBUILD_CPPFLAGS)
ccflags-y += -Iarch/x86/kvm

CFLAGS_x86.o := -I.
CFLAGS_svm.o := -I.
CFLAGS_vmx.o := -I.

CFLAGS_kvm_main.o := -Ivirt/kvm

KVM := ../../../../virt/kvm

kvm-y			+= kvm_main.o $(KVM)/coalesced_mmio.o \
				$(KVM)/eventfd.o $(KVM)/irqchip.o $(KVM)/vfio.o
kvm-$(CONFIG_KVM_ASYNC_PF)	+= $(KVM)/async_pf.o

kvm-y			+= x86.o mmu.o $(b)/emulate.o $(b)/i8259.o $(b)/irq.o $(b)/lapic.o \
			   $(b)/i8254.o $(b)/ioapic.o $(b)/irq_comm.o $(b)/cpuid.o $(b)/pmu.o $(b)/mtrr.o \
			   $(b)/hyperv.o $(b)/page_track.o $(b)/debugfs.o

kvm-y			+= kvm-l4.o
kvm-intel-y		+= vmx.o $(b)/pmu_intel.o
kvm-amd-y		+= svm.o $(b)/pmu_amd.o

obj-$(CONFIG_KVM)	+= kvm.o
obj-$(CONFIG_KVM_INTEL)	+= kvm-intel.o
obj-$(CONFIG_KVM_AMD)	+= kvm-amd.o

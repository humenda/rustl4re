# SPDX-License-Identifier: GPL-2.0
#
# Makefile for local APIC drivers and for the IO-APIC code
#

# Leads to non-deterministic coverage that is not a function of syscall inputs.
# In particualr, smp_apic_timer_interrupt() is called in random places.
KCOV_INSTRUMENT		:= n

obj-$(CONFIG_X86_LOCAL_APIC)	+= apic.o apic_common.o apic_noop.o ipi.o vector.o
obj-y				+= hw_nmi.o

obj-$(CONFIG_X86_IO_APIC)	+= io_apic.o
obj-$(CONFIG_PCI_MSI)		+= msi.o
obj-$(CONFIG_SMP)		+= ipi.o

ifeq ($(CONFIG_X86_64),y)
ifeq ($(CONFIG_L4),)
# APIC probe will depend on the listing order here
obj-$(CONFIG_X86_NUMACHIP)	+= apic_numachip.o
obj-$(CONFIG_X86_UV)		+= x2apic_uv_x.o
obj-$(CONFIG_X86_X2APIC)	+= x2apic_phys.o
obj-$(CONFIG_X86_X2APIC)	+= x2apic_cluster.o
obj-y				+= apic_flat_64.o
else
obj-y				+= apic_l4x.o
endif # L4
endif

# APIC probe will depend on the listing order here
obj-$(CONFIG_X86_BIGSMP)	+= bigsmp_32.o

# For 32bit, probe_32 need to be listed last
obj-$(CONFIG_X86_LOCAL_APIC)	+= probe_$(BITS).o

# -------------------------------------------
include arch/l4/Makefile.lib

$(call L4X_FILEREF, ../../../../x86/kernel/apic, \
       apic_common apic_noop apic_flat_64 probe_$(BITS) hw_nmi)

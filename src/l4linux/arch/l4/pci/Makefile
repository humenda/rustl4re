# SPDX-License-Identifier: GPL-2.0

ifneq ($(CONFIG_L4_VPCI),)
  obj-y             += l4vpci.o
  obj-$(CONFIG_X86) += i386.o fixup.o
else
 ifneq ($(CONFIG_X86),)
  include $(srctree)/arch/x86/pci/Makefile
  KBUILD_CPPFLAGS += -I$(srctree)/arch/x86/pci
 endif
endif

include arch/l4/Makefile.lib

$(call L4X_FILEREF, ../../$(L4X_SRCARCH)/pci, \
                    common direct fixup i386 irq legacy mmconfig    \
                    mmconfig_32 mmconfig_64 acpi init early amd_bus \
                    bus_numa intel_mid_pci)

# SPDX-License-Identifier: GPL-2.0-only
#
# Makefile for the ia32 kernel emulation subsystem.
#

obj-$(CONFIG_IA32_EMULATION) := ia32_signal.o

obj-$(CONFIG_IA32_AOUT) += ia32_aout.o

audit-class-$(CONFIG_AUDIT) := audit.o
obj-$(CONFIG_IA32_EMULATION) += $(audit-class-y)

include arch/l4/Makefile.lib

$(call L4X_FILEREF, ../../x86/ia32, \
       ia32_signal ia32_aout audit)

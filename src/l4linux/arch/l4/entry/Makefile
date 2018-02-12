# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the x86 low level entry code
#

OBJECT_FILES_NON_STANDARD_entry_64_compat.o := y

CFLAGS_syscall_64.o		+= $(call cc-option,-Wno-override-init,)
CFLAGS_syscall_32.o		+= $(call cc-option,-Wno-override-init,)
obj-y				:= entry_$(BITS).o thunk_$(BITS).o syscall_$(BITS).o
obj-y				+= common.o

obj-y				+= vdso/
obj-y				+= vsyscall/

obj-$(CONFIG_IA32_EMULATION)	+= entry_64_compat.o syscall_32.o

# -------------------------------------------
KBUILD_AFLAGS			+= -Iarch/x86/entry

include arch/l4/Makefile.lib


$(call L4X_FILEREF, ../../x86/entry, \
       thunk_$(BITS) syscall_$(BITS) syscall_32)

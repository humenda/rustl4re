#
# Makefile for the x86 low level vsyscall code
#
obj-y					:= vsyscall_gtod.o

obj-$(CONFIG_X86_VSYSCALL_EMULATION)	+= vsyscall_64.o vsyscall_emu_64.o

# -------------------------------------------
KBUILD_CFLAGS			+= -Iarch/x86/entry/vsyscall

include arch/l4/Makefile.lib

$(call L4X_FILEREF, ../../../x86/entry/vsyscall, \
       vsyscall_emu_64 vsyscall_gtod)

PKGDIR		?= ../..
L4DIR		?= $(PKGDIR)/../..

include $(L4DIR)/mk/Makeconf
ifneq ($(SYSTEM),)
ifeq ($(GCCVERSION),2.95)
TARGET		= x86emu.o
else
all install::
	@echo -e $(EMPHSTART)"  ==> X86emu stub module is not built since Linux-2.2 code requires gcc-2.95!"$(EMPHSTOP)
endif
endif
MODE		= l4linux_kern
SYSTEMS		= x86-l4f

SRC_C		= emu.c
LIBS		= -lx86emu
CPPFLAGS	= -DMODULE -DNO_SYS_HEADERS -fomit-frame-pointer
WARNINGS	= -Wall -Wstrict-prototypes
LDFLAGS		= -r -nostdlib -nostartfiles

INSTALLFILE_BIN_LOCAL = $(STRIP) --strip-unneeded $(1) \
			-o $(INSTALLDIR_LOCAL)/$(TARGET)

include $(L4DIR)/mk/prog.mk

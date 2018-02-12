PKGDIR        ?= ..
L4DIR         ?= $(PKGDIR)/../..

DDE_SYSTEMS    = x86 arm

TARGET         = libdde_linux26_input.o.a
REQUIRES_LIBS  = dde-linux26_char

SRC_C		= contrib/mouse/alps.c \
		  contrib/mouse/synaptics.c \
		  contrib/mouse/trackpoint.c \
		  contrib/mouse/logips2pp.c \
		  contrib/mouse/psmouse-base.c \
		  contrib/keyboard/atkbd.c \
		  contrib/serio/libps2.c \
		  contrib/serio/serio.c \
		  contrib/input-compat.c \
		  contrib/input.c \
		  contrib/ff-core.c \
		  evdev.c \
		  dde26_input.c \
		  libinput.c

ifneq ($(ARCH), arm)
SRC_C		+= contrib/mouse/lifebook.c
endif

SRC_C_x86-l4f   += $(SRCALL) contrib/serio/i8042.c
SRC_C_amd64-l4f += $(SRCALL) contrib/serio/i8042.c
SRC_C_arm-l4f   += $(SRCALL) contrib/serio/ambakmi.c

PRIVATE_INCDIR 	+= $(SRC_DIR)/contrib

LIBCINCDIR  = -nostdinc $(I_GCCINCDIR)
DEFINES     = -D__KERNEL__ -DDDE_LINUX $(KBUILD_DEFINES)
CPPFLAGS 	+= $(KBUILD_CPPFLAGS)

ifeq ($(ARCH), arm)
ARCH_DIR = arch/arm
DEFINES += -D__LINUX_ARM_ARCH__=6
endif

include $(PKGDIR)/../dde/linux26/Makeconf

include $(L4DIR)/mk/lib.mk

CFLAGS := $(filter-out -std=gnu99,$(CFLAGS))

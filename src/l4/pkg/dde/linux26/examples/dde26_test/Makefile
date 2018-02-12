PKGDIR        ?= ../../..
L4DIR         ?= $(PKGDIR)/../..

DDE_SYSTEMS    = x86 arm

REQUIRES_LIBS  = slab ddekit dde-linux26 dde-linux26_pci libio l4re_c-util l4util

ifeq ($(ARCH), arm)
ARCH_DIR = arch/arm
DEFINES += -D__LINUX_ARM_ARCH__=6
endif

-include $(PKGDIR_OBJ)/Makeconf

TARGET         = dde26_test

SRC_C          = main.c

# DDE configuration
include $(PKGDIR)/linux26/Makeconf

include $(L4DIR)/mk/prog.mk

CFLAGS := $(filter-out -std=gnu99,$(CFLAGS))

PKGDIR         ?= ../../..
L4DIR          ?= $(PKGDIR)/../..

SYSTEMS         = x86-l4f

SRC_C           = main.c

DEFAULT_RELOC   = 0x00730000
TARGET          = fer_test

MODE            = l4env_minimal

LIBS           += -lferret_util

include $(L4DIR)/mk/prog.mk

PKGDIR         ?= ../../..
L4DIR          ?= $(PKGDIR)/../..

SYSTEMS         = x86-l4f

SRC_C           = main.c

DEFAULT_RELOC   = 0x00680000
TARGET          = fer_list_demo


REQUIRES_LIBS+= ferret-producer ferret-common libstdc++

include $(L4DIR)/mk/prog.mk

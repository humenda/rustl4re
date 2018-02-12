PKGDIR         ?= ../../..
L4DIR          ?= $(PKGDIR)/../..

SYSTEMS         = x86-l4f

SRC_C           = main.c

#DEFAULT_RELOC   = 0x00680000
TARGET          = ferret_l4lx_list

MODE            = l4linux

LIBS            = -lferret_l4lx_client -lferret_util
LIBCLIBS        =

include $(L4DIR)/mk/prog.mk

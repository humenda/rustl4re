PKGDIR         ?= ../..
L4DIR          ?= $(PKGDIR)/../..

SYSTEMS         = x86-l4f

SRC_C           = arping.c

#DEFAULT_RELOC   = 0x00680000
TARGET          = l4lx_arping

MODE            = l4linux

LIBS            = -lferret_l4lx_client -lferret_util -lnet -lpcap
LIBCLIBS        =

include $(L4DIR)/mk/prog.mk

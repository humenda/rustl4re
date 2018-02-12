PKGDIR         ?= ../../..
L4DIR          ?= $(PKGDIR)/../..

SYSTEMS         = x86-l4f

SRC_C           = main.c

#DEFAULT_RELOC   = 0x00680000
TARGET          = ferret_histo_mon

MODE            = l4linux

LIBS            = -lferret_consumer -lferret_comm -lferret_fpages
LIBCLIBS        =

include $(L4DIR)/mk/prog.mk

PKGDIR         ?= ../../..
L4DIR          ?= $(PKGDIR)/../..

SYSTEMS         = x86-l4f

SRC_C           = main.c uu.c

DEFAULT_RELOC   = 0x006f0000
TARGET          = fer_l4lx_vtm

MODE            = l4env_minimal

LIBS           += -lferret_monitor -lferret_consumer -lferret_util -ll4z    \
                  -lferret_comm -lferret_fpages -lferret_local_names

PRIVATE_INCDIR += $(PKGDIR_ABS)/examples/monitors/common
vpath uu.c $(PKGDIR_ABS)/examples/monitors/common

include $(L4DIR)/mk/prog.mk

PKGDIR         ?= ../../..
L4DIR          ?= $(PKGDIR)/../..

SYSTEMS         = x86-l4f

SRC_C           = main.c

DEFAULT_RELOC   = 0x00630000
TARGET          = fer_histo_demo

MODE            = l4env_minimal

LIBS           += -lferret_client -lferret_producer -lferret_comm           \
                  -lferret_fpages -lferret_local_names

include $(L4DIR)/mk/prog.mk

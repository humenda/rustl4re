PKGDIR         ?= ../../..
L4DIR          ?= $(PKGDIR)/../..

SYSTEMS         = x86-l4f

SRC_C           = main.c

DEFAULT_RELOC   = 0x00680000
TARGET          = fer_dp_test

MODE            = l4env_minimal

LIBS           += -lferret_monitor -lferret_client -lferret_producer        \
                  -lferret_consumer -lferret_util -lferret_comm             \
                  -lferret_fpages -lferret_fpages -lferret_local_names

#OPTS = -O3 -fomit-frame-pointer

include $(L4DIR)/mk/prog.mk

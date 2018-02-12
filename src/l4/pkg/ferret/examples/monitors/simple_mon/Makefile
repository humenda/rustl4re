PKGDIR         ?= ../../..
L4DIR          ?= $(PKGDIR)/../..

SYSTEMS         = x86-l4f
SRC_C           = main.c
TARGET          = fer_simple_mon
LIBS           += -lferret_monitor -lferret_consumer -lferret_util          \
                  -lferret_comm -lferret_fpages -lferret_local_names

include $(L4DIR)/mk/prog.mk

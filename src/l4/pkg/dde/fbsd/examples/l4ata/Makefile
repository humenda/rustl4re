PKGDIR     ?= ../../..
L4DIR      ?= $(PKGDIR)/../..

TARGET      = l4ata
SYSTEMS     = x86-l4f

SRC_C      += $(addprefix src/, main.c disk.c reqloop.c request.c)

LIBS       += -lbddf_busdriver_if_ipc -lbddf_timebase -Wl,--whole-archive -ldde-ata -ldde-bsd -Wl,--no-whole-archive -ldde_kit -lio -lomega0

PRIVATE_INCDIR += $(PKGDIR)/examples/l4ata/include

# configuring options
#
# build rt_mon sensors
# DEFINES        += -DRTMON
# LIBS           += -lrt_mon_client

include $(L4DIR)/mk/prog.mk

PKGDIR ?= ../..
L4DIR ?= $(PKGDIR)/../..

SYSTEMS = x86-l4f arm-l4f

# the name of your library
TARGET	= libnetboot.a
LINK_INCR = $(TARGET)
LINK_INCR_ONLYGLOBSYMFILE_$(LINK_INCR) = $(SRC_DIR)/symbols.all

# list your .c or .cc files here
SRC_C	= config.c nic.c ankh.c l4_timer.c
# reset C99 variable to force old mode
CFLAGS_C99 =

PRIVATE_INCDIR += $(SRC_DIR)

PC_FILENAME = ankh_netboot

include $(L4DIR)/mk/lib.mk

CFLAGS += -include ankh_defs.h -Wno-unused -DMDEBUG

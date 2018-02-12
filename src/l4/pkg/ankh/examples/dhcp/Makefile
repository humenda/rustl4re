PKGDIR          ?= ../..
L4DIR           ?= $(PKGDIR)/../..

TARGET           = dhcp_clnt
SYSTEMS          = x86-l4f arm-l4f
SRC_C            = main.c 
REQUIRES_LIBS    = ankh_netboot ankh libpthread

PRIVATE_INCDIR += $(SRC_DIR)/../../lib/netboot

include $(L4DIR)/mk/prog.mk

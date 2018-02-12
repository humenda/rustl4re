PKGDIR          ?= ../../..
L4DIR           ?= $(PKGDIR)/../..

TARGET           = libvscreen.a
SYSTEMS          = x86-linux
MODE             = host

DEFINES         += -DSOCKETAPI -DDICE_DEFAULT_PORT=htons\(\(int\)dice_server_param\)
PRIVATE_INCDIR   = $(PKGDIR)/lib/dope/include \
                   $(PKGDIR)/include \
                   $(DICE_INCDIR)

SRC_C            = vscreen.c util.c
CLIENTIDL        = vscr.idl

vpath util.c $(PKGDIR)/lib/vscreen/common

include $(L4DIR)/mk/lib.mk


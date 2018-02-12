PKGDIR          ?= ../../..
L4DIR           ?= $(PKGDIR)/../..

TARGET           = liblinuxdope.a
SYSTEMS          = x86-linux
MODE             = host

DEFINES         += -DSOCKETAPI -DDICE_DEFAULT_PORT=htons\(\(int\)dice_server_param\)

PRIVATE_INCDIR   = $(PKGDIR)/lib/dope/include \
                   $(PKGDIR)/server/include \
                   $(PKGDIR)/include \
                   $(DICE_INCDIR)

SRC_C            = func.c init.c sync.c listener.c main.c freeport.c events.c \
                   dope-client.c dopeapp-server.c

CLIENTIDL        = dope.idl dopeapp.idl
SERVERIDL        = dopeapp.idl

vpath main.c $(PKGDIR)/lib/dope/common
vpath func.c $(PKGDIR)/lib/dope/dice
vpath events.c $(PKGDIR)/lib/dope/dice

include $(L4DIR)/mk/lib.mk

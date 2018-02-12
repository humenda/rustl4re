PKGDIR          ?= ../..
L4DIR           ?= $(PKGDIR)/../..

TARGET           = linuxdope
SYSTEMS          = x86-linux
MODE             = host

DEFINES         += -DSOCKETAPI -DDICE_DEFAULT_PORT=htons\(\(int\)dice_server_param\)

SERVERIDL        = dope.idl vscr.idl
CLIENTIDL        = dopeapp.idl

PRIVATE_LIBDIR   = $(PKGDIR_OBJ)/server/common/OBJ-$(SYSTEM) \
                   $(PKGDIR_OBJ)/server/gfx/OBJ-$(SYSTEM)

LIBS             = -lcommon -lgfx -lpthread -lSDL

PRIVATE_INCDIR   = $(PKGDIR)/include \
                   $(PKGDIR)/server/include \
                   $(DICE_INCDIR)

SRC_C            = messenger.c dummies.c     \
                   scrdrv.c server.c    startup.c     \
                   thread.c timer.c     vscr_server.c \
                   input.c  sharedmem.c

vpath messenger.c $(PKGDIR)/server/common

#include $(L4DIR)/pkg/petze/mk/petze.mk
include $(L4DIR)/mk/prog.mk

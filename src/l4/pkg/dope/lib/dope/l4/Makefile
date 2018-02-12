PKGDIR        ?= ../../..
L4DIR         ?= $(PKGDIR)/../..

TARGET         = lib4dope.a
REQUIRES_LIBS  = libpthread l4re_c-util
PRIVATE_INCDIR = $(PKGDIR)/lib/dope/include \
                 $(PKGDIR)/server/include \
                 $(PKGDIR)/include
SRC_C          = sync.c init.c main.c
SRC_CC         = func.cc

vpath %        $(PKGDIR)/lib/dope/common


include $(L4DIR)/mk/lib.mk


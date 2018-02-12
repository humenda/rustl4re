PKGDIR ?= ../..
L4DIR  ?= $(PKGDIR)/../..

TARGET         = libexpat2.a libexpat2.so
CONTRIB_INCDIR = expat2
DEFINES        = -DHAVE_EXPAT_CONFIG_H
WARNINGS       = $(WARNINGS_MEDIUM)
PRIVATE_INCDIR = $(SRC_DIR)
SRC_C          = xmlparse.c xmltok.c xmlrole.c

vpath %.c $(PKGDIR)/lib/contrib/lib

include $(L4DIR)/mk/lib.mk

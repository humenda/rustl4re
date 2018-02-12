PKGDIR          ?= ../..
L4DIR           ?= $(PKGDIR)/../..

TARGET          = libiniparser.a libiniparser.so
SYSTEMS         = $(SYSTEMS_PLAIN)
CONTRIB_INCDIR  = libiniparser

SRC_C           = dictionary.c iniparser.c

vpath %.c $(SRC_DIR)/../contrib/src

include $(L4DIR)/mk/lib.mk

PKGDIR ?= ../..
L4DIR  ?= $(PKGDIR)/../..

TARGET  	= libl4re-wrapper.a
PC_FILENAME	= libl4re-wrapper
SRC_C = debug.c env.c ipc.c mem.c

REQUIRES_LIBS += libpthread l4re_c l4re_c-util

include $(L4DIR)/mk/lib.mk


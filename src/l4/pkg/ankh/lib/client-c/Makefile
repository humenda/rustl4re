PKGDIR?= ../..
L4DIR ?= $(PKGDIR)/../..

SYSTEMS       = x86-l4f arm-l4f
PC_FILENAME   = ankh
REQUIRES_LIBS = shmc_ringbuf l4re-util
TARGET        = lib$(PKGNAME)-c.a lib$(PKGNAME)-c.so
SRC_CC        = lib.cc

include $(L4DIR)/mk/lib.mk

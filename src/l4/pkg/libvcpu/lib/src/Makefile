PKGDIR  ?= ../..
L4DIR   ?= $(PKGDIR)/../..

PC_FILENAME   = libvcpu
TARGET        = libvcpu.a libvcpu.so
SRC_CC        = vcpu.cc ARCH-$(ARCH)/arch.cc
REQUIRES_LIBS := l4re-util

include $(L4DIR)/mk/lib.mk

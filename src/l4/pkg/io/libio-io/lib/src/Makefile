PKGDIR         ?= ../..
L4DIR          ?= $(PKGDIR)/../../..

TARGET         := libio-io.a libio-io.so
SRC_CC         := io.cc
CONTRIB_INCDIR  = libio-io
REQUIRES_LIBS  := libio-vbus
PC_FILENAME     = libio-io

include $(L4DIR)/mk/lib.mk

CXXFLAGS += -fvisibility=hidden
CFLAGS   += -fvisibility=hidden


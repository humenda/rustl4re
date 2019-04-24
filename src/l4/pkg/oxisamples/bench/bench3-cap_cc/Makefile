PKGDIR          ?= .
L4DIR           ?= $(PKGDIR)/../../../..

CPPFLAGS += -std=c++17 -DTEST_CAP=1
TARGET                   = bench3-client_cc
SRC_CC = ../bench1-primitive_cc/client.cc
REQUIRES_LIBS            = cxx_libc_io cxx_io l4re_c l4re_c-util

include $(L4DIR)/mk/prog.mk

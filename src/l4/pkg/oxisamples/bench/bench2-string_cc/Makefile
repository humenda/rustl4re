PKGDIR          ?= .
L4DIR           ?= $(PKGDIR)/../../../..

CCFLAGS += --std=c++11 -DTEST_STRING
TARGET                   = bench2-client_cc
SRC_CC = client.cc
REQUIRES_LIBS            = cxx_libc_io cxx_io

include $(L4DIR)/mk/prog.mk

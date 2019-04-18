PKGDIR          ?= .
L4DIR           ?= $(PKGDIR)/../../../..

CCFLAGS += --std=c++11
TARGET                   = bench1-client_cc bench1-server_cc
SRC_CC_bench1-client_cc = client.cc
SRC_CC_bench1-server_cc = server.cc
REQUIRES_LIBS            = cxx_libc_io cxx_io l4re_c l4re_c-util

include $(L4DIR)/mk/prog.mk

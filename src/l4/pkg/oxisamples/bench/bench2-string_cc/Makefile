PKGDIR          ?= .
L4DIR           ?= $(PKGDIR)/../../../..

CCFLAGS += --std=c++11
TARGET                   = bench2-client_cc bench2-server_cc
SRC_CC_bench2-client_cc = client.cc
SRC_CC_bench2-server_cc = server.cc
REQUIRES_LIBS            = cxx_libc_io cxx_io

include $(L4DIR)/mk/prog.mk

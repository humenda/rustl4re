PKGDIR ?=	../../..
L4DIR ?=	$(PKGDIR)/..

TARGET                = romain_hello romain_hello_c
SRC_CC_romain_hello = main.cc
SRC_C_romain_hello_c = cmain.c

PRIVATE_INCDIR = $(SRC_DIR)/../../server/src/

CXXFLAGS += $(call checkcxx,-std=c++11)
REQUIRES_LIBS = libstdc++ libevbuf_c

SYSTEMS = x86-l4f

include $(L4DIR)/mk/prog.mk

PKGDIR ?=	../../..
L4DIR ?=	$(PKGDIR)/..

SYSTEMS = x86-l4f

TARGET = romain_superpages
SRC_CC = main.cc

REQUIRES_LIBS = libstdc++

CXXFLAGS += -I$(SRC_DIR)/../../server/src/
include $(L4DIR)/mk/prog.mk

#LDFLAGS += -lpthread_rep -luc_c -levbuf_c

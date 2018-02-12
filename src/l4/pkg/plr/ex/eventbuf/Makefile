PKGDIR      ?= ../..
L4DIR       ?= $(PKGDIR)/../..

TARGET      = romain_eventbuf

SYSTEMS     = x86-l4f

# list your .c or .cc files here
SRC_C       =
SRC_CC      = main.cc

CXXFLAGS += -I$(SRC_DIR)/../../server/src/
CXXFLAGS += $(call checkcxx,-std=c++11)

vpath %.cc $(SRC_DIR)/../../server/src/

# list requirements of your program here
REQUIRES_LIBS = libstdc++ libvcpu libudis86 zlib libuu

include $(L4DIR)/mk/prog.mk

PKGDIR      ?= ../..
L4DIR       ?= $(PKGDIR)/../..

TARGET      = vcpu_tne

SYSTEMS     = x86-l4f

# list your .c or .cc files here
SRC_C       =
SRC_CC      = vcpu.cc emulation.cc

CXXFLAGS += -I$(SRC_DIR)/../../server/src/ -std=c++11

vpath %.cc $(SRC_DIR)/../../server/src/

# list requirements of your program here
REQUIRES_LIBS = libstdc++ libvcpu libudis86

include $(L4DIR)/mk/prog.mk

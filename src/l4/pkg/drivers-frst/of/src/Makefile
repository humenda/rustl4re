PKGDIR ?= ../..
L4DIR  ?= $(PKGDIR)/../..

SRC_CC     := of.cc
SYSTEMS     = $(SYSTEMS_PLAIN)
TARGET      = libdrivers_of.a
PC_FILENAME = drivers_of

PRIVATE_INCDIR += $(SRC_DIR)/../include 

include $(L4DIR)/mk/lib.mk

CXXFLAGS += -DL4_NO_RTTI -fno-exceptions -fno-rtti

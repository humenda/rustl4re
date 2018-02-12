PKGDIR          ?= ../..
L4DIR           ?= $(PKGDIR)/../..

TARGET           = morping morpong
SYSTEMS          = x86-l4f arm-l4f
REQUIRES_LIBS    = ankh libboost-lite libpthread
SRC_CC_morping   = ping.cc
SRC_CC_morpong   = ping.cc

include $(L4DIR)/mk/prog.mk

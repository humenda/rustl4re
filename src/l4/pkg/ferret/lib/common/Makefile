PKGDIR   ?= ../..
L4DIR    ?= $(PKGDIR)/../..

# all the producer stuff is inline, so no lib is needed
SRC_CC_libferret_util.a     = pack.cc unpack.cc util.cc
SRC_CC_libferret_comm.a     = comm.cc

# convert.cc switched off until we know about the tbuf interface

TARGET    = libferret_util.a libferret_comm.a
REQUIRES_LIBS = libpthread libstdc++

PC_FILENAME = ferret-common

include $(L4DIR)/mk/lib.mk

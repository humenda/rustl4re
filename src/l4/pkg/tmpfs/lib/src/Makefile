PKGDIR  ?= ../..
L4DIR   ?= $(PKGDIR)/../..

TARGET	      = libl4revfs-fs-tmpfs.a libl4revfs-fs-tmpfs.so
LINK_INCR     = libl4revfs-fs-tmpfs.a
PC_FILENAME   = libl4revfs-fs-tmpfs
SRC_CC        = fs.cc

include $(L4DIR)/mk/lib.mk

CXXFLAGS += -fvisibility=hidden
CFLAGS   += -fvisibility=hidden


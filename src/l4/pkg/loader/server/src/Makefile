PKGDIR		?= ../..
L4DIR		?= $(PKGDIR)/../..

TARGET		= loader
SRC_CC		= main.cc slab_alloc.cc \
                  region.cc app_task.cc name_space.cc \
		  log.cc global.cc \
		  sched_proxy.cc alloc.cc
LDFLAGS +=
#CPPFLAGS += -fPIC

REQUIRES_LIBS  := libloader libkproxy cxx_libc_io cxx_io

include $(L4DIR)/mk/prog.mk

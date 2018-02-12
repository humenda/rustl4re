PKGDIR		?= ../..
L4DIR		?= $(PKGDIR)/../..

TARGET		= rtc
SRC_CC_x86	= x86.cc ux.cc # ux must be last so that the driver is probed first
SRC_CC_amd64	= x86.cc
SRC_CC		= main.cc $(SRC_CC_$(ARCH))
SRC_C		= $(SRC_C_$(ARCH))
REQUIRES_LIBS   = libio cxx_libc_io cxx_io libstdc++
REQUIRES_LIBS_x86 = lxfuxlibc

include $(L4DIR)/mk/prog.mk

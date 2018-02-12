PKGDIR		?= ../..
L4DIR		?= $(PKGDIR)/../..

TARGET		= libx86emu_int10.a
SYSTEMS		= x86-l4f amd64-l4f
SRC_CC          = int10.cc
CFLAGS		= -fomit-frame-pointer
DEFINES         = -DUSE_IO
REQUIRES_LIBS   = libio x86emu libedid
PC_FILENAME     = x86emu_int10

include $(L4DIR)/mk/lib.mk

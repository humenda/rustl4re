PKGDIR		?= ../..
L4DIR		?= $(PKGDIR)/../..

TARGET		= x86emu_test
SYSTEMS		= x86-l4f

SRC_C		= emu.c
PRIVATE_INCDIR	= ../../include
CFLAGS		= -fomit-frame-pointer
LIBS		= -lx86emu

DEBUG		= 0

include $(L4DIR)/mk/prog.mk

PKGDIR?= ../..
L4DIR ?= $(PKGDIR)/../..

# the name of your library
TARGET = libddekit.a

# list your .c files here
SRC_C = \
		block.c \
		condvar.c \
		init.c \
		interrupt.c \
		lock.c \
		panic.c \
		pgtab.c \
		printf.c \
		resources.c \
		timer.c \
		thread.c \
		semaphore.c

SRC_CC = malloc.cc \
		 memory.cc \
		 pci.cc

REQUIRES_LIBS := libio slab l4re_c-util libpthread

PC_FILENAME = ddekit

#interrupt.c
#resources.c
#timer.c

include $(L4DIR)/mk/lib.mk

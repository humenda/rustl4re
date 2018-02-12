PKGDIR         ?= ../../..
L4DIR          ?= $(PKGDIR)/../..

SYSTEMS         = x86-l4f

SRC_C           = main.c

#DEFAULT_RELOC   = 0x00680000
TARGET          = ferret_pthread_test

MODE            = l4linux

OPTS = -O3 -fno-strict-aliasing $(CARCHFLAGS_$(ARCH)) $(CARCHFLAGS_$(ARCH)_$(CPU)) -fomit-frame-pointer

LIBS            = -lpthread -lferret_init -lferret_producer
LIBCLIBS        =

include $(L4DIR)/mk/prog.mk

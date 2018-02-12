PKGDIR  ?= ../../..
L4DIR   ?= $(PKGDIR)/../../..

TARGET = libocaml_camlrun.a
NOTARGETSTOINSTALL = y

CONTRIBDIR = $(PKGDIR)/contrib

vpath %.c $(CONTRIBDIR)/byterun

PRIVATE_INCDIR = $(CONTRIBDIR)/byterun \
		 $(SRC_DIR)/.. \
                 $(SRC_DIR)/../../ARCH-$(ARCH)/config


BYTECCCOMPOPTS=-fno-defer-pop -Wall -D_FILE_OFFSET_BITS=64 -D_REENTRANT
CFLAGS=-DCAML_NAME_SPACE $(BYTECCCOMPOPTS) $(IFLEXDIR)

SRC_C = interp.c misc.c stacks.c fix_code.c startup.c \
        freelist.c major_gc.c minor_gc.c memory.c alloc.c roots.c globroots.c \
        fail.c signals.c signals_byt.c printexc.c backtrace.c \
        compare.c ints.c floats.c str.c array.c io.c extern.c intern.c \
        hash.c sys.c meta.c parsing.c gc_ctrl.c terminfo.c md5.c obj.c \
        lexing.c callback.c debugger.c weak.c compact.c finalise.c custom.c \
        dynlink.c unix.c main.c


include $(L4DIR)/mk/lib.mk

# contrib code: hide all the warnings
CFLAGS := $(CFLAGS) -Wno-missing-prototypes -Wno-missing-declarations \
          -Wno-sign-compare -Wno-unused -Wno-implicit \
	  -Wno-strict-prototypes

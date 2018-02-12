PKGDIR  ?= ../..
L4DIR   ?= $(PKGDIR)/../../..

SYSTEMS     = $(filter-out arm-l4f mips-l4f,$(SYSTEMS_ABI))

# no .so as amd64.S doesn't seem to like it
TARGET      = libocaml_asmrun.a
PC_FILENAME = libocaml_asmrun

CONTRIBDIR = $(PKGDIR)/contrib

# order matter, i.e. asmrun needs to go before byterun
vpath %.S $(CONTRIBDIR)/asmrun
vpath %.c $(CONTRIBDIR)/asmrun
vpath %.c $(CONTRIBDIR)/byterun

PRIVATE_INCDIR = $(SRC_DIR)/../byterun \
                 $(CONTRIBDIR)/byterun \
                 $(SRC_DIR) \
                 $(SRC_DIR)/../ARCH-$(ARCH)/config

OCAML_ARCH_FILE_x86   = i386
OCAML_ARCH_FILE_amd64 = amd64
OCAML_ARCH_FILE_arm   = arm
OCAML_ARCH_FILE_mips  = mips
OCAML_ARCH_FILE_ppc32 = power-elf
OCAML_ARCH_FILE_sparc = sparc
OCAML_ARCH_FILE       = $(OCAML_ARCH_FILE_$(ARCH))

NATIVECCCOMPOPTS=-Wall -D_FILE_OFFSET_BITS=64 -D_REENTRANT

DEFINES_x86   = -DTARGET_i386 -DSYS_linux_elf
DEFINES_amd64 = -DTARGET_amd64 -DSYS_linux
DEFINES_arm   = -DTARGET_arm
DEFINES_mips  = -DTARGET_mips
DEFINES_ppc32 = -DTARGET_power -DSYS_elf
DEFINES_sparc = -DTARGET_sparc
DEFINES       = -DCAML_NAME_SPACE -DNATIVE_CODE \
                $(DEFINES_$(ARCH)) $(NATIVECCCOMPOPTS)

# arm.s contains some fp instructions, set version explicitely to have it
# compile, it will trap at runtime as nobody will emulate it
ASFLAGS_arm.S     = -mfpu=fpa -mabi=apcs-gnu

SRC_C = startup.c main.c fail.c roots.c globroots.c signals.c signals_asm.c \
        misc.c freelist.c major_gc.c minor_gc.c memory.c alloc.c compare.c \
	ints.c floats.c str.c array.c io.c extern.c intern.c hash.c sys.c \
	parsing.c gc_ctrl.c terminfo.c md5.c obj.c lexing.c printexc.c \
	callback.c weak.c compact.c finalise.c custom.c unix.c backtrace.c \
	natdynlink.c
SRC_S = $(OCAML_ARCH_FILE).S

include $(L4DIR)/mk/lib.mk

# contrib code: hide all the warnings
CFLAGS := $(CFLAGS) -Wno-missing-prototypes -Wno-missing-declarations \
          -Wno-sign-compare -Wno-unused -Wno-implicit \
	  -Wno-strict-prototypes

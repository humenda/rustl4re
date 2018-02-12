PKGDIR   ?= ../..
L4DIR    ?= $(PKGDIR)/../..

SRC_C_libferret_gcc_instrument.a     = instrument.c
SRC_C_libferret_gcc_instrument_std.a = std_instrument.c

MODE      = l4env_minimal

SYSTEMS   = x86-l4f

TARGET    = libferret_gcc_instrument.a libferret_gcc_instrument_std.a

include $(L4DIR)/mk/lib.mk

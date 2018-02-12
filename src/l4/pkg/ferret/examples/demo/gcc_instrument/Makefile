PKGDIR         ?= ../../..
L4DIR          ?= $(PKGDIR)/../..

SYSTEMS         = x86-l4f

SRC_C_fer_gcc_inst_gen = gen.c
SRC_C_fer_gcc_inst_std = std.c

DEFAULT_RELOC   = 0x00680000
TARGET          = fer_gcc_inst_gen fer_gcc_inst_std

MODE            = l4env_minimal

CFLAGS         += -finstrument-functions

LIBS_fer_gcc_inst_gen += -lferret_gcc_instrument -lferret_client          \
                         -lferret_producer -lferret_util -lferret_comm    \
                         -lferret_fpages -lferret_local_names

LIBS_fer_gcc_inst_std += -lferret_gcc_instrument_std -lferret_client      \
                         -lferret_producer -lferret_util -lferret_comm    \
                         -lferret_fpages -lferret_local_names

include $(L4DIR)/mk/prog.mk

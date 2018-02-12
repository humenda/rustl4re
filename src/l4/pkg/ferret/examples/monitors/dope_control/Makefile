PKGDIR         ?= ../../..
L4DIR          ?= $(PKGDIR)/../..

SYSTEMS         = x86-l4f

PRIVATE_INCDIR  =+ $(PKGDIR_ABS)/examples/monitors/dope_control .

SRC_C           = main.c

DEFAULT_RELOC   = 0x00344000
TARGET          = fer_dope_ctrl

MODE            = l4env_minimal

LIBS            = -lferret_client -lferret_producer -lferret_util -ll4dope  \
                  -lferret_comm -lferret_fpages -lferret_local_names

include $(L4DIR)/mk/prog.mk

vpath main_window.dpe $(PKGDIR_ABS)/examples/monitors/dope_control
vpath main_window.dpe $(PKGDIR_OBJ)/examples/monitors/dope_control/OBJ-$(SYSTEM)

main_window.i: main_window.dpe
	$(L4DIR)/tool/gen_dopecode/gen_dopecode $^ > $@

main.o: main_window.i

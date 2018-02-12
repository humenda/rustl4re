PKGDIR   ?= ../..
L4DIR    ?= $(PKGDIR)/../..

TARGET    = libvg_static_memcheck.o.a

SRC_C = mc_replace_strmem.c \
		vg_replace_malloc.c \
		tbd.c

VG_DIR = $(PKGDIR)/src/valgrind-3.6.0-svn

vpath %.c $(VG_DIR)/memcheck \
		  $(VG_DIR)/coregrind/m_replacemalloc

PRIVATE_INCDIR += $(VG_DIR)/include
PRIVATE_INCDIR += $(VG_DIR)/VEX/pub
PRIVATE_INCDIR += $(VG_DIR)/coregrind

DEFINES = -DVGO_l4re -DVGP_x86_l4re -DVGA_x86

CPPFLAGS += -Wno-pointer-sign -Wno-unused-parameter

include $(L4DIR)/mk/lib.mk

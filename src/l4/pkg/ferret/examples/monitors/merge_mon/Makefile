PKGDIR         ?= ../../..
L4DIR          ?= $(PKGDIR)/../..

SYSTEMS         = x86-l4f

PRIVATE_INCDIR += $(PKGDIR_ABS)/examples/merge_mon .

SRC_C           = main.c buffer.c uu.c gui.c\
#gui.c buffer.c uu.c net.c poll.c
DPE_gui.c       = main_window.dpe
TARGET          = fer_merge_mon

REQUIRES_LIBS= ferret-consumer ferret-common dope zlib libstdc++ l4re_c-util


include $(L4DIR)/mk/prog.mk

PRIVATE_INCDIR += $(PKGDIR_ABS)/examples/monitors/common
vpath uu.c $(PKGDIR_ABS)/examples/monitors/common


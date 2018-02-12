PKGDIR          ?= ../..
L4DIR           ?= $(PKGDIR)/../..

TARGET          = libudis86.a libudis86.so
SYSTEMS         = $(SYSTEMS_PLAIN)
PC_FILENAME     = libudis86
CONTRIB_INCDIR  = libudis86

CONTRIB_PATH    = $(SRC_DIR)/../contrib
PRIVATE_INCDIR  = $(CONTRIB_PATH)/libudis86 $(OBJ_DIR)/../include

SRC_C           = itab.c decode.c udis86.c \
                  syn-att.c syn-intel.c syn.c

vpath %.c $(CONTRIB_PATH)/libudis86
vpath itab.c $(OBJ_DIR)/../include

include $(L4DIR)/mk/lib.mk

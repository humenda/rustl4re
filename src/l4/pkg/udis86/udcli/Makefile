PKGDIR	?= ..
L4DIR	?= $(PKGDIR)/../..

TARGET         = udcli
REQUIRES_LIBS  = libudis86
SRC_C          = udcli.c
PRIVATE_INCDIR = $(SRC_DIR)

vpath %.c $(SRC_DIR)/../lib/contrib/udcli/

include $(L4DIR)/mk/prog.mk

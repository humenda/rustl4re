PKGDIR ?= ../..
L4DIR  ?= $(PKGDIR)/../..

DDE_SYSTEMS      = arm x86
TARGET           = dde_inputtst
REQUIRES_LIBS    = ddekit dde-linux26 dde-linux26_char dde-libinput l4re_c-util
SRC_C            = main.c

include $(PKGDIR)/../dde/linux26/Makeconf

include $(L4DIR)/mk/prog.mk

PKGDIR		?= ../..
L4DIR		?= $(PKGDIR)/../..

TARGET		= ex_start-with-exc
SYSTEMS		= x86-l4f arm-l4f
SRC_C		= main.c
REQUIRES_LIBS   = l4re_c-util
DEPENDS_PKGS    = $(REQUIRES_LIBS)

include $(L4DIR)/mk/prog.mk

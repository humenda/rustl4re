PKGDIR		?= ../..
L4DIR		?= $(PKGDIR)/../..

SUBDIRS		= idl dumplog

TARGET		= logcon
LIBS		= -u putchar -u puts -lcontxt -lcon -lconstream-server
SERVERIDL       = log.idl logcon.idl
IDL_PKGDIR	= $(PKGDIR_OBJ)/../log \
		  $(PKGDIR_OBJ)/examples/logcon

SRC_C		= main.c

include $(L4DIR)/mk/prog.mk

$(foreach arch,$(TARGET_SYSTEMS), $(OBJ_DIR)/OBJ-$(arch)): idl

all:: $(SUBDIRS)

dumplog: idl

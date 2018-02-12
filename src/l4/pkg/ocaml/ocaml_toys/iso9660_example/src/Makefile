PKGDIR	?= ../..
L4DIR	?= $(PKGDIR)/../../..

TARGET	      = ocaml_ex_iso9660
SYSTEMS       = x86-l4f
SRC_CC	      = main.cc
REQUIRES_LIBS = libocaml_iso9660 libc_be_sig libc_support_misc

include $(PKGDIR)/../ocaml/mk/defs.mk
include $(PKGDIR)/../ocaml/mk/version.mk
include $(L4DIR)/mk/prog.mk

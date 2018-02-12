PKGDIR	?= ../..
L4DIR	?= $(PKGDIR)/../../..

TARGET	      = ocaml_ex_ext2
SYSTEMS       = x86-l4f
SRC_CC	      = main.cc
REQUIRES_LIBS = libocaml_ext2 libc_be_sig libc_support_misc

include $(PKGDIR)/../ocaml/mk/defs.mk
include $(PKGDIR)/../ocaml/mk/version.mk
include $(L4DIR)/mk/prog.mk

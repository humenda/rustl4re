PKGDIR        ?= ../../..
L4DIR         ?= $(PKGDIR)/../../..

TARGET         = libocaml_ext2.a
PC_FILENAME    = libocaml_ext2
LINK_INCR      = libocaml_ext2.a
SYSTEMS        = x86-l4f
SRC_CC         = wrapper.cc
SRC_ML         = ext2.ml
OBJS           = ext2.o
REQUIRES_LIBS  = libocaml_asmrun

include $(PKGDIR)/../ocaml/mk/defs.mk
include $(PKGDIR)/../ocaml/mk/version.mk
include $(L4DIR)/mk/lib.mk
include $(PKGDIR)/../ocaml/mk/mlbuild.mk

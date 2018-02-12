PKGDIR        ?= ../../..
L4DIR         ?= $(PKGDIR)/../../..

TARGET         = libocaml_iso9660.a
PC_FILENAME    = libocaml_iso9660
LINK_INCR      = libocaml_iso9660.a
SYSTEMS        = x86-l4f
SRC_CC         = wrapper.cc
SRC_ML         = iso9660.ml
OBJS           = iso9660.o
REQUIRES_LIBS  = libocaml_asmrun

include $(PKGDIR)/../ocaml/mk/defs.mk
include $(PKGDIR)/../ocaml/mk/version.mk
include $(L4DIR)/mk/lib.mk
include $(PKGDIR)/../ocaml/mk/mlbuild.mk

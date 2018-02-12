PKGDIR	= .
L4DIR	?= $(PKGDIR)/../../..

TARGET = libext2 ext2_example libiso9660 iso9660_example

include $(PKGDIR)/../ocaml/mk/defs.mk
include $(OCAML)/mk/checktools.mk

include $(L4DIR)/mk/subdir.mk

ext2_example: libext2
iso9660_example : libiso9660

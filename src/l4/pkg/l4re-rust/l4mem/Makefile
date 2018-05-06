PKGDIR	= .
L4DIR	?= $(PKGDIR)/../../..

BINDGENARGS += --blacklist-type=L4_PAGESHIFT
TARGET 		= libl4mem-rust.rlib
PC_FILENAME	= libl4mem-rust
SRC_RS = lib.rs


REQUIRES_LIBS    = l4re_c


include $(L4DIR)/mk/lib.mk

PKGDIR	= .
L4DIR	?= $(PKGDIR)/../../../..

BINDGENARGS += --blacklist-type=L4_PAGESHIFT --blacklist-type 'l4_addr_t'
TARGET 		= libl4re_sys-rust.rlib
PC_FILENAME	= l4re-sys-rust
SRC_RS = lib.rs


REQUIRES_LIBS    = l4re_c


include $(L4DIR)/mk/lib.mk

PKGDIR	= .
L4DIR	?= $(PKGDIR)/../../../../..

TARGET		= ox_rcalc-client
SRC_RS		= src/main.rs

REQUIRES_LIBS    = libdl libpthread libc_be_sig l4re_c-util libc_support_misc \
				   libl4re-wrapper l4-sys-rust l4-rust l4re-sys-rust

include $(L4DIR)/mk/prog.mk

PKGDIR	= .
L4DIR	?= $(PKGDIR)/../../../../..

TARGET		= shm_rust_base-server
SRC_RS		= server.rs

REQUIRES_LIBS    = libdl libpthread libc_be_sig l4re_c-util libc_support_misc l4re-rust l4-sys-rust l4-rust libl4re-wrapper l4re_c

include $(L4DIR)/mk/prog.mk

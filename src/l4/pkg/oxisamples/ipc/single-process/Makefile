PKGDIR	= .
L4DIR	?= $(PKGDIR)/../../../..

TARGET		= basic_ipc
SRC_RS		= main.rs

DEPENDS_PKGS = $(REQUIRES_LIBS)
REQUIRES_LIBS    = libdl l4re_c-util l4re libpthread l4sys libc_be_sig l4re_c-util libc_support_misc libl4re-wrapper l4-sys-rust
include $(L4DIR)/mk/prog.mk

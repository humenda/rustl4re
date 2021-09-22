PKGDIR	= .
L4DIR	?= $(PKGDIR)/../../../../..

TARGET		= two_task_ipc-client
SRC_RS		= client.rs

DEPENDS_PKGS = $(REQUIRES_LIBS)
REQUIRES_LIBS    = libdl libpthread libc_be_sig l4re_c-util libc_support_misc l4-sys-rust l4-rust l4re-rust l4_derive-rust libl4re-wrapper
include $(L4DIR)/mk/prog.mk

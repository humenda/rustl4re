PKGDIR	?= .
L4DIR	?= $(PKGDIR)/../../../..

RSFLAGS += --cfg test_str
TARGET = bench2-client_str
SRC_RS = ../bench1-primitive/client/client.rs

REQUIRES_LIBS    = libdl libpthread libc_be_sig l4re_c-util libc_support_misc \
				   libl4re-wrapper l4-sys-rust l4-rust l4re-rust l4_derive-rust

include $(L4DIR)/mk/prog.mk

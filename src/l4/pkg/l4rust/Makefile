PKGDIR	= .
L4DIR	?= $(PKGDIR)/../..

TARGET = l4re-rust l4-rust l4-sys-rust libl4re-wrapper

l4-sys-rust: libl4re-wrapper
l4-rust: l4-sys-rust
l4re-rust: l4-rust l4_derive-rust

include $(L4DIR)/mk/subdir.mk

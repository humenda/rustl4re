PKGDIR ?= .
L4DIR  ?= $(PKGDIR)/../../../..

RSFLAGS += -Zunstable-options --pretty=expanded

TARGET  		= libl4-rust.rlib
SRC_RS   		= lib.rs

DEPENDS_PKGS    = l4sys l4-sys-rust
REQUIRES_LIBS   = l4sys libl4re-wrapper l4-sys-rust
include $(L4DIR)/mk/lib.mk

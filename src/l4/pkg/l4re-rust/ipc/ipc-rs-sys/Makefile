PKGDIR ?= .
L4DIR  ?= $(PKGDIR)/../../../..

BINDGENARGS +=  \
		--whitelist-type '.*' \
		--whitelist-function '.*l4.*' \
		--whitelist-type '.*std::type_info' \
		--whitelist-type ".*Cap.*" \
		--blacklist-type='.*'L4::Ipc::Msg::'.*'_xmit \
		--blacklist-type='.*'L4::Ipc::Msg::Svr_xmit \
		--blacklist-type='.*'L4::Ipc::Msg::Clnt_xmit \
		--no-copy '.*Type_info'
	# Doc:
	# --whitelist-function is required so that the C functions from the
	#  l4-ipc-wrapper are found
TARGET  		= libipc_sys-rust.rlib
SRC_RS   		= lib.rs

DEPENDS_PKGS    = l4sys
REQUIRES_LIBS   = l4sys libl4re-wrapper
include $(L4DIR)/mk/lib.mk

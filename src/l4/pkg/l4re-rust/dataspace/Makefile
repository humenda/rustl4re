PKGDIR	= .
L4DIR	?= $(PKGDIR)/../../..

#BINDGENARGS += --blacklist-type=root::L4::Ipc::Msg::Detail::Svr_xmit \
#		--blacklist-type=L4::Ipc::Msg::Detail::Svr_xmit \
#		--blacklist-type=L4::Ipc::Msg::Svr_xmit \
#		--blacklist-type=L4::Ipc::Msg::Clnt_xmit \
#		--blacklist-type=std::'.*' \
#		--whitelist-type "std::type_info" \
#		--whitelist-type "L4Re::Dataspace"
#integral_constant'.*'
TARGET 		= libdataspace-rust.rlib
PC_FILENAME	= libdataspace-rust
SRC_RS = lib.rs

include $(L4DIR)/mk/lib.mk

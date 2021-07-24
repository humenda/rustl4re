
lib-$(CONFIG_L4_SERVER)       += server.o
lib-$(CONFIG_L4_SERVER_INPUT) += input-srv.o
lib-$(CONFIG_L4_SERVER_FB)    += fb-srv.o
lib-$(CONFIG_L4_SERVER_FDX)   += fdx-srv.o

ifneq ($(L4OBJ),)
include $(L4OBJ)/l4defs.mk.inc
endif

L4_EXT_CFLAGS := $(call L4_BID_PKG_CONFIG_CALL,$(L4OBJ),--cflags --define-variable=libc_variant=libc,$(L4_REQUIRED_MODS))

# see scripts/ for more
define rule_l4re_cxx_o_cc
	$(call cmd,checksrc) 
	$(call cmd_and_fixdep,l4re_cxx_o_cc)
	$(call cmd,gen_ksymdeps)
	$(call cmd,checkdoc)
	$(call cmd,objtool)
	$(call cmd,modversions_c)
	$(call cmd,record_mcount)
endef

L4RE_CXX_CALL = $(L4_CXX) -Wp,-MD,$(depfile) -c -DL4SYS_USE_UTCB_WRAP=1 \
                -D__KERNEL__ -I$(srctree)/arch/l4/include \
                $(L4_CPPFLAGS_MINIMAL) $(L4_CXXFLAGS_MINIMAL) $(L4_EXT_CFLAGS) -fno-exceptions

quiet_cmd_l4re_cxx_o_cc = C++ $(quiet_modtag)  $@

cmd_l4re_cxx_o_cc = $(L4RE_CXX_CALL) -c -o $@ $<

$(obj)/%.o: $(src)/%.cc FORCE
	$(call cmd,force_checksrc)
	$(call if_changed_rule,l4re_cxx_o_cc)

# Makefile snippet to generate l4defs.inc file
# vim:set ft=make:
#

PKGDIR = .
SYSTEMS = $(BUILD_ARCH)-$(BUILD_ABI)

OBJ_DIR = $(SRC_DIR)/build

# the target file
L4DEF_FILE_MK ?= l4defs.mk.inc
L4DEF_FILE_SH ?= l4defs.sh.inc

do_output_all   = echo '$(1) = $($(1))' >> $(L4DEF_FILE_MK); echo '$(1)="$($(1))"' >> $(L4DEF_FILE_SH);
do_output_mk    = echo '$(1) = $($(1))' >> $(L4DEF_FILE_MK);

ifeq ($(CALLED_FOR),static)

MODE = static

ifneq ($(SYSTEM),)

L4_SYSTEM                    = $(ARCH)_$(CPU)
L4_CC                        = $(CC)
L4_CXX                       = $(CXX)
L4_LD                        = $(LD)
L4_LDFLAGS_LD_STATIC         = $(filter-out -l%,$(BID_LDFLAGS_FOR_LINKING_LD))
L4_LDFLAGS_GCC_STATIC        = $(filter-out -l%,$(BID_LDFLAGS_FOR_LINKING_GCC))
L4_LDFLAGS_DYNAMIC_LINKER_LD = $(LDFLAGS_DYNAMIC_LINKER)
L4_LDFLAGS_DYNAMIC_LINKER_GCC= $(LDFLAGS_DYNAMIC_LINKER_GCC)
L4_LIBDIRS                   = $(addprefix -L,$(L4LIBDIR))
L4_CPPFLAGS                  = $(CPPFLAGS)
L4_CFLAGS                    = $(CFLAGS)
L4_CXXFLAGS                  = $(CXXFLAGS)
L4_GCCSYSLIBDIRS             = $(GCCSYSLIBDIRS)
L4_GCCLIB                    = $(GCCLIB)
L4_GCCLIB_SO                 = $(GCCLIB_SO)
L4_GCCLIB_EH                 = $(GCCLIB_EH)
L4_LDS_stat_bin              = $(LDS_stat_bin)
L4_BID_STACK_ADDR            = $(L4_STACK_ADDR)
L4_BID_STACK_SIZE            = $(L4_STACK_SIZE)
L4_BID_KIP_ADDR              = $(L4_KIP_ADDR)
L4_BID_KIP_OFFS_SYS_INVOKE   = $(L4_KIP_OFFS_SYS_INVOKE)
L4_BID_KIP_OFFS_SYS_DEBUGGER = $(L4_KIP_OFFS_SYS_DEBUGGER)
L4_BID_PKG_CONFIG            = $(PKG_CONFIG)
L4_BID_PKG_CONFIG_CALL       = $(value BID_PKG_CONFIG)
L4_BID_PKG_CONFIG_FAILED     = $(value BID_PKG_CONFIG_FAILED)

L4_BID_BENDER_LINK    = $(value BID_BENDER_LINK)
L4_BID_BENDER_DEFINES = $(value BID_BENDER_DEFINES)


all::
	@echo -e $(EMPHSTART)"Creating $(L4DEF_FILE_MK)"$(EMPHSTOP)
	@echo "# Definitions for 'make' from L4 BID"                            > $(L4DEF_FILE_MK)
	@echo "# vim:se ft=make:"                                              >> $(L4DEF_FILE_MK)
	@echo "# --- Automatically created, do not modify ---"                 >> $(L4DEF_FILE_MK)
	@:
	@echo -e $(EMPHSTART)"Creating $(L4DEF_FILE_SH)"$(EMPHSTOP)
	@echo "# Definitions for 'sh' from L4 BID"                              > $(L4DEF_FILE_SH)
	@echo "# vim:se ft=sh:"                                                >> $(L4DEF_FILE_SH)
	@echo "# --- Automatically created, do not modify ---"                 >> $(L4DEF_FILE_SH)
	@echo ""                                                               >> $(L4DEF_FILE_SH)
	@echo "l4_bid_call_pkgconfig()"                                        >> $(L4DEF_FILE_SH)
	@echo "{"                                                              >> $(L4DEF_FILE_SH)
	@echo "  obj_base=\$$1; shift; "                                       >> $(L4DEF_FILE_SH)
	@echo "  PKG_CONFIG_LIBDIR=\$$obj_base/pc PKG_CONFIG_PATH= \\"         >> $(L4DEF_FILE_SH)
	@echo "  \$$L4_BID_PKG_CONFIG \\"                                      >> $(L4DEF_FILE_SH)
	@echo "   --define-variable=incdir=\$$obj_base/include/contrib \\"     >> $(L4DEF_FILE_SH)
	@echo "   \"\$$@\""                                                    >> $(L4DEF_FILE_SH)
	@echo "}"                                                              >> $(L4DEF_FILE_SH)
	@echo ""                                                               >> $(L4DEF_FILE_SH)
	@:
	@$(call do_output_all,L4_SYSTEM)
	@$(call do_output_all,L4_CC)
	@$(call do_output_all,L4_CXX)
	@$(call do_output_all,L4_LD)
	@$(call do_output_all,L4_LDFLAGS_LD_STATIC)
	@$(call do_output_all,L4_LDFLAGS_GCC_STATIC)
	@$(call do_output_all,L4_LDFLAGS_DYNAMIC_LINKER_LD)
	@$(call do_output_all,L4_LDFLAGS_DYNAMIC_LINKER_GCC)
	@$(call do_output_all,L4_LIBDIRS_R)
	@$(call do_output_all,L4_LIBDIRS)
	@$(call do_output_all,L4_GCCSYSLIBDIRS)
	@$(call do_output_all,L4_CPPFLAGS)
	@$(call do_output_all,L4_CFLAGS)
	@$(call do_output_all,L4_CXXFLAGS)
	@$(call do_output_all,L4_GCCLIB)
	@$(call do_output_all,L4_GCCLIB_EH)
	@$(call do_output_all,L4_LDS_stat_bin)
	@$(call do_output_all,L4_BID_STACK_ADDR)
	@$(call do_output_all,L4_BID_KIP_ADDR)
	@$(call do_output_all,L4_BID_KIP_OFFS_SYS_INVOKE)
	@$(call do_output_all,L4_BID_KIP_OFFS_SYS_DEBUGGER)
	@$(call do_output_all,L4_BID_PKG_CONFIG)
	@$(call do_output_mk,L4_BID_PKG_CONFIG_CALL)
	@$(call do_output_mk,L4_BID_PKG_CONFIG_FAILED)
	@$(call do_output_mk,L4_BID_BENDER_LINK)
	@$(call do_output_mk,L4_BID_BENDER_DEFINES)
	@$(foreach v,LIBCINCDIR REQUIRES_LIBS LDSCRIPT LDFLAGS, \
	  $(call do_output_all,$(v)_all_static) $(call do_output_all,$(v)_all_shared))
else
all::
endif

include $(L4DIR)/mk/prog.mk

endif # called for static

#   ----------------------------------------------------

ifeq ($(CALLED_FOR),minimal)

MODE = sigma0

ifneq ($(SYSTEM),)
REQUIRES_LIBS = libc_minimal

L4_CPPFLAGS_MINIMAL = $(CPPFLAGS)
L4_CFLAGS_MINIMAL   = $(CFLAGS)
L4_CXXFLAGS_MINIMAL = $(CXXFLAGS)

all::
	@$(call do_output_all,L4_CPPFLAGS_MINIMAL)
	@$(call do_output_all,L4_CFLAGS_MINIMAL)
	@$(call do_output_all,L4_CXXFLAGS_MINIMAL)
else
all::
endif

include $(L4DIR)/mk/prog.mk

endif # called for minimal

#   ----------------------------------------------------

ifeq ($(CALLED_FOR),shared)

MODE = shared

ifneq ($(SYSTEM),)

L4_LDFLAGS_LD_SHARED  = $(filter-out -l%,$(BID_LDFLAGS_FOR_LINKING_LD))
L4_LDFLAGS_GCC_SHARED = $(filter-out -l%,$(BID_LDFLAGS_FOR_LINKING_GCC))
L4_LDS_so             = $(LDS_so)
L4_LDS_dyn_bin        = $(LDS_dyn_bin)

all::
	@$(call do_output_all,L4_LDFLAGS_LD_SHARED)
	@$(call do_output_all,L4_LDFLAGS_GCC_SHARED)
	@$(call do_output_all,L4_LDS_so)
	@$(call do_output_all,L4_LDS_dyn_bin)

else
all::
endif

include $(L4DIR)/mk/prog.mk

endif # called for shared

#   ----------------------------------------------------

ifeq ($(CALLED_FOR),sharedlib)

ifneq ($(SYSTEM),)

NOTARGETSTOINSTALL = y
L4_CRT0_SO = $(CRT0)
L4_CRTN_SO = $(CRTN)

all::
	@$(call do_output_all,L4_CRT0_SO)
	@$(call do_output_all,L4_CRTN_SO)
else
all::
endif

include $(L4DIR)/mk/lib.mk

endif # called for sharedlib

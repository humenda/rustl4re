# This file contains helpers to ease the development with a Cargo-based project.

RLIB_BASE=$(OBJ_BASE)/lib/rustlib

is_no_procmacro = $(if $(wildcard $(RLIB_BASE)/$(1)/host-deps/*$(1:%-rust=%)*.so),,$(1))
is_procmacro = $(if $(wildcard $(RLIB_BASE)/$(1)/host-deps/*$(1:%-rust=%)*.so),$(1))

# check each library whether it's a proc macro library and only keep those which
# aren't
RS_LINK_LIBRARIES=$(foreach LIB,$(strip $(filter %rust,$(REQUIRES_LIBS))),$(call is_no_procmacro,$(LIB)))

# paths to all Rust dependencies
_RUST_DEP_PATHS = $(addprefix $(RLIB_BASE)/,$(RS_LINK_LIBRARIES))
# add paths for proc macro crates so that the compiler can locate the shared
# objects and load them
PROC_MACRO_LIBDIRS = $(strip \
		$(foreach LIB, $(strip $(filter %rust,$(REQUIRES_LIBS))),\
			$(strip $(patsubst %,-L $(RLIB_BASE)/%/host-deps/,\
			$(call is_procmacro,$(LIB))))))

# allow rustc to find l4-bender
export PATH := $(PATH):$(L4DIR)/tool/bin


# paths to all source files so that make can track changes of files (different
# to C/C++, where each source file is contained in SRC_C(C).
SRC_FILES = $(strip $(shell find $(abspath $(abspath $(SRC_DIR))/$(dir $(firstword $(SRC_RS)))) -name '*.rs'))


#emit=@echo export $(1)='$($(1)')
#
#emit-cargo-vars:
#	$(call emit,RSFLAGS)
#	$(call emit,CARGO_BUILD_TARGET_DIR)
#	$(call emit,CARGO_BUILD_RUSTFLAGS)
#	$(call emit,L4_INCLUDE_DIRS)
#	$(call emit,L4_BENDER_ARGS)
#	$(call emit,L4_LD_OPTIONS)
#	@gallow rustc to find l4-bender
#	$(call emit,PATH)
#	@echo CARGO_BUILD_TARGET=$(RUST_TARGET)
#g--manifest-path=$(PKGDIR)/Cargo.toml
#

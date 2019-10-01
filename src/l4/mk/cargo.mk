# This file contains helpers to ease the development with a Cargo-based project.

# Base directory for Rust libraries. Rust libraries are located in lib/rustlib
# and each rlib is contained within its own subdirectory. This allows keeping
# the required library dependency information available.
RLIB_BASE=$(OBJ_BASE)/lib/rustlib

# Paths to all source files so that make can track changes of files (different
# to C/C++, where each source file is contained in SRC_C(C).
# ToDo: This is a hack and assumes that source files are directly in the package
# directory.
SRC_FILES = $(strip $(shell find $(abspath $(abspath $(SRC_DIR))/$(dir $(firstword $(SRC_RS)))) -name '*.rs'))

# Read Cargo.toml, grep for proc-macro to identify a procmacro crate.
# Edge case: "proc-macro = false", but configuring this is nonsense.
is_pkg_procmacro = $(if $(findstring proc-macro,$(file <$(PKGDIR)/Cargo.toml)),$(PKGNAME))

# Test whether (an already built) library is aproc macro crate
# This uses the PC file to read this information from.
is_procmacro_dep = $(if $(findstring proc-macro,$(file <$(OBJ_BASE)/pc/$(1).pc)),$(1))
# Test whether (an already built) library is not aproc macro crate
is_no_procmacro_dep = $(if $(findstring proc-macro,$(file <$(OBJ_BASE)/pc/$(1).pc)),,$(1))

# check each library whether it's a proc macro library and only keep those which
# aren't
RS_LINK_LIBRARIES=$(foreach LIB,$(strip $(filter %rust,$(REQUIRES_LIBS))),$(call is_no_procmacro_dep,$(LIB)))

# Paths to all Rust dependencies
_RUST_DEP_PATHS = $(addprefix $(RLIB_BASE)/,$(RS_LINK_LIBRARIES))

# Add paths for proc macro crates so that the compiler can locate the shared
# objects and load them
PROC_MACRO_LIBDIRS = $(strip \
		$(foreach LIB, $(strip $(filter %rust,$(REQUIRES_LIBS))),\
			$(strip $(patsubst %,-L $(RLIB_BASE)/%/host-deps/,\
			$(call is_procmacro_dep,$(LIB))))))

# Allow rustc to find l4-bender as its linker
export PATH := $(PATH):$(L4DIR)/tool/bin

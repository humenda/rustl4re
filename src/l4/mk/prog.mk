# -*- Makefile -*-
#
# DROPS (Dresden Realtime OPerating System) Component
#
# Makefile-Template for binary directories
#
# Makeconf is used, see there for further documentation
# install.inc is used, see there for further documentation
# binary.inc is used, see there for further documentation

ifeq ($(origin _L4DIR_MK_PROG_MK),undefined)
_L4DIR_MK_PROG_MK=y

ROLE = prog.mk

include $(L4DIR)/mk/Makeconf
$(GENERAL_D_LOC): $(L4DIR)/mk/prog.mk

define copy_stripped_binary
  $(call create_dir,$(dir $(2))/.debug); \
  ln -sf $(call absfilename,$(1)) $(dir $(2))/.debug/$(1); \
  $(OBJCOPY) --strip-unneeded --add-gnu-debuglink=$(1) \
             $(1) $(2) >/dev/null 2>&1 \
    || ln -sf $(call absfilename,$(1)) $(2); \
  chmod 755 $(2)
endef

# define INSTALLDIRs prior to including install.inc, where the install-
# rules are defined.
ifneq ($(filter host targetsys,$(MODE)),)
INSTALLDIR_BIN		?= $(DROPS_STDDIR)/bin/$(MODE)
INSTALLDIR_BIN_LOCAL	?= $(OBJ_BASE)/bin/$(MODE)
else
INSTALLDIR_BIN		?= $(DROPS_STDDIR)/bin/$(subst -,/,$(SYSTEM))
INSTALLDIR_BIN_LOCAL	?= $(OBJ_BASE)/bin/$(subst -,/,$(SYSTEM))
endif
ifneq ($(CONFIG_BID_STRIP_PROGS),)
INSTALLFILE_BIN 	?= $(call copy_stripped_binary,$(1),$(2))
INSTALLFILE_BIN_LOCAL 	?= $(call copy_stripped_binary,$(1),$(2))
else
INSTALLFILE_BIN 	?= $(INSTALL) -m 755 $(1) $(2)
INSTALLFILE_BIN_LOCAL 	?= $(INSTALL) -m 755 $(1) $(2)
endif

INSTALLFILE		= $(INSTALLFILE_BIN)
INSTALLDIR		= $(INSTALLDIR_BIN)
INSTALLFILE_LOCAL	= $(INSTALLFILE_BIN_LOCAL)
INSTALLDIR_LOCAL	= $(INSTALLDIR_BIN_LOCAL)

# our mode
MODE 			?= static

# include all Makeconf.locals, define common rules/variables
include $(L4DIR)/mk/binary.inc

ifneq ($(SYSTEM),) # if we have a system, really build

TARGET_STANDARD := $(TARGET) $(TARGET_$(OSYSTEM))
TARGET_PROFILE := $(addsuffix .pr,$(filter $(BUILD_PROFILE),$(TARGET)))

$(call GENERATE_PER_TARGET_RULES,$(TARGET_STANDARD))
$(call GENERATE_PER_TARGET_RULES,$(TARGET_PROFILE),.pr)

TARGET	+= $(TARGET_$(OSYSTEM)) $(TARGET_PROFILE)

# define some variables different for lib.mk and prog.mk
ifeq ($(MODE),shared)
LDFLAGS += $(LDFLAGS_DYNAMIC_LINKER)
endif
ifeq ($(CONFIG_BID_GENERATE_MAPFILE),y)
LDFLAGS += -Map $(strip $@).map
endif
LDFLAGS += $(addprefix -L, $(PRIVATE_LIBDIR) $(PRIVATE_LIBDIR_$(OSYSTEM)) $(PRIVATE_LIBDIR_$@) $(PRIVATE_LIBDIR_$@_$(OSYSTEM)))

# here because order of --defsym's is important
ifeq ($(MODE),l4linux)
  L4LX_USER_KIP_ADDR = 0xbfdfd000
  LDFLAGS += --defsym __L4_KIP_ADDR__=$(L4LX_USER_KIP_ADDR) \
             --defsym __l4sys_invoke_direct=$(L4LX_USER_KIP_ADDR)+$(L4_KIP_OFFS_SYS_INVOKE) \
             --defsym __l4sys_debugger_direct=$(L4LX_USER_KIP_ADDR)+$(L4_KIP_OFFS_SYS_DEBUGGER)
  CPPFLAGS += -DL4SYS_USE_UTCB_WRAP=1
else
ifneq ($(HOST_LINK),1)
  LDFLAGS += --defsym __L4_KIP_ADDR__=$(L4_KIP_ADDR) \
	     --defsym __L4_STACK_ADDR__=$(L4_STACK_ADDR)
endif
endif

ifneq ($(HOST_LINK),1)
  # linking for our L4 platform
  LDFLAGS += $(addprefix -L , $(L4LIBDIR))
  LDFLAGS += $(addprefix -T , $(LDSCRIPT))
  LDFLAGS += --warn-common
else
  # linking for some POSIX platform
  LDFLAGS += $(addprefix -PC,$(REQUIRES_LIBS))
  ifeq ($(MODE),host)
    # linking for the build-platform
    LDFLAGS += -L$(OBJ_BASE)/lib/host
    LDFLAGS += $(LIBS)
  else
    # linking for L4Linux, we want to look for Linux-libs before the L4-libs
    LDFLAGS += $(GCCSYSLIBDIRS)
    LDFLAGS += $(addprefix -L, $(L4LIBDIR))
    LDFLAGS += $(LIBS)
  endif
endif

LDFLAGS += $(LDFLAGS_$@)

include $(L4DIR)/mk/install.inc

#VPATHEX = $(foreach obj, $(OBJS), $(firstword $(foreach dir, \
#          . $(VPATH),$(wildcard $(dir)/$(obj)))))

# target-rule:

# Make looks for obj-files using VPATH only when looking for dependencies
# and applying implicit rules. Though make adapts its automatic variables,
# we cannot use them: The dependencies contain files which have not to be
# linked to the binary. Therefore the foreach searches the obj-files like
# make does, using the VPATH variable.
# Use a surrounding strip call to avoid ugly linebreaks in the commands
# output.

# Dependencies: When we have ld.so, we use MAKEDEP to build our
# library dependencies. If not, we fall back to LIBDEPS, an
# approximation of the correct dependencies for the binary. Note, that
# MAKEDEP will be empty if we dont have ld.so, LIBDEPS will be empty
# if we have ld.so.

ifeq ($(CONFIG_HAVE_LDSO),)
LIBDEPS = $(foreach file, \
                    $(patsubst -l%,lib%.a,$(filter-out -L%,$(LDFLAGS))) \
                    $(patsubst -l%,lib%.so,$(filter-out -L%,$(LDFLAGS))),\
                    $(word 1, $(foreach dir, \
                           $(patsubst -L%,%,\
                           $(filter -L%,$(LDFLAGS) $(L4ALL_LIBDIR))),\
                      $(wildcard $(dir)/$(file)))))
endif

DEPS	+= $(foreach file,$(TARGET), $(call BID_LINK_DEPS,$(file)))

LINK_PROGRAM-C-host-1   := $(CC)
LINK_PROGRAM-CXX-host-1 := $(CXX)

bid_call_if = $(if $(2),$(call $(1),$(2)))

LINK_PROGRAM  := $(call bid_call_if,BID_LINK_MODE_host,$(LINK_PROGRAM-C-host-$(HOST_LINK)))
ifneq ($(SRC_CC),)
LINK_PROGRAM  := $(call bid_call_if,BID_LINK_MODE_host,$(LINK_PROGRAM-CXX-host-$(HOST_LINK)))
endif

BID_LDFLAGS_FOR_LINKING_LD  = $(LDFLAGS)
BID_LDFLAGS_FOR_GCC         = $(filter     -static -shared -nostdlib -Wl$(BID_COMMA)% -L% -l% -PC%,$(LDFLAGS))
BID_LDFLAGS_FOR_LD          = $(filter-out -static -shared -nostdlib -Wl$(BID_COMMA)% -L% -l% -PC%,$(LDFLAGS))
BID_LDFLAGS_FOR_LINKING_GCC = $(addprefix -Wl$(BID_COMMA),$(BID_LDFLAGS_FOR_LD)) $(BID_LDFLAGS_FOR_GCC)

ifeq ($(LINK_PROGRAM),)
LINK_PROGRAM  := $(BID_LINK)
BID_LDFLAGS_FOR_LINKING = $(call BID_mode_var,NOPIEFLAGS) -MD -MF $(call BID_link_deps_file,$@) \
                          $(addprefix -PC,$(REQUIRES_LIBS)) $(BID_LDFLAGS_FOR_LINKING_LD)
else
BID_LDFLAGS_FOR_LINKING = $(call BID_mode_var,NOPIEFLAGS) -MD -MF $(call BID_link_deps_file,$@) \
                          $(if $(HOST_LINK_TARGET),$(CCXX_FLAGS)) $(BID_LDFLAGS_FOR_LINKING_GCC)
endif

ifeq ($(SRC_RS),)
$(TARGET): $(OBJS) $(LIBDEPS)
	@$(LINK_MESSAGE)
	$(VERBOSE)$(call MAKEDEP,$(INT_LD_NAME),,,ld) $(LINK_PROGRAM) -o $@ $(BID_LDFLAGS_FOR_LINKING) $(OBJS) $(LIBS) $(EXTRA_LIBS)
	$(if $(BID_GEN_CONTROL),$(VERBOSE)echo "Requires: $(REQUIRES_LIBS)" >> $(PKGDIR)/Control)
	$(if $(BID_POST_PROG_LINK_MSG_$@),@$(BID_POST_PROG_LINK_MSG_$@))
	$(if $(BID_POST_PROG_LINK_$@),$(BID_POST_PROG_LINK_$@))
	@$(BUILT_MESSAGE)
else # rust target rule
ifneq ($(firstword $(notdir $(LINK_PROGRAM))),l4-bender)
$(error Rust compilation is only supported for l4-bender, got $(firstword $(notdir $(LINK_PROGRAM))))
endif
# Rust's build tool is Cargo, which calls the compiler rustc. Rustc compiles the
# code and calls the linker. A linker variant "l4-bender" has been added to
# Rustc. To pass BID arguments to l4-bender, L4_BENDER_ARGS and L4_LD_OPTIONS
# are read by the compiler and incoorperated into the l4-bender command line.
# Additional Rustc flags can be passed using the CARGO_BUILD_RUSTFLAGS variable,
# read by cargo.

# strip first word (l4-bender path) and last word (-- delimiter)
L4_BENDER_ARGS_HEADLESS=$(strip $(LINK_PROGRAM:$(firstword $(LINK_PROGRAM))%=%))
export L4_BENDER_ARGS=$(strip $(L4_BENDER_ARGS_HEADLESS:%$(lastword $(LINK_PROGRAM))=%))
export L4_LD_OPTIONS=$(filter-out -PClib%rust,$(BID_LDFLAGS_FOR_LINKING)) $(LIBS) $(EXTRA_LIBS)
# allow rustc to find l4-bender
export PATH := $(PATH):$(L4DIR)/tool/bin
export CARGO_BUILD_RUSTFLAGS=$(strip $(patsubst -D%,--cfg=%,$(filter -D%,$(CPPFLAGS))) \
		$(addprefix -L, $(PRIVATE_LIBDIR) $(PRIVATE_LIBDIR_$(OSYSTEM)) $(PRIVATE_LIBDIR_$@) $(PRIVATE_LIBDIR_$@_$(OSYSTEM))) \
		-L $(OBJ_BASE)/lib -L $(OBJ_BASE)/) $(RSFLAGS)

# manifest path: location of the Cargo.toml, which should reside in the PKGDIR
# $PATH has to be extended to include l4-bender
$(strip $(TARGET)): $(SRC_RS)
	@$(LINK_MESSAGE)
	$(VERBOSE)$(call MAKEDEP,$(INT_LD_NAME),,,ld) \
		cargo build $(if $(VERBOSE),,-v) \
			$(if $(strip $(DEBUG)),--debug,--release) \
			--target=$(RUST_TARGET) \
			--manifest-path=$(PKGDIR)/Cargo.toml
	@$(BUILT_MESSAGE)
	$(VERBOSE)cp $(RUST_RESULT_DIR)/$(strip $(TARGET)) $(CARGO_BUILD_TARGET_DIR)
endif

endif	# architecture is defined, really build

-include $(DEPSVAR)
.PHONY: all clean cleanall config help install oldconfig txtconfig
help::
	@echo "  all            - compile and install the binaries"
ifneq ($(SYSTEM),)
	@echo "                   to $(INSTALLDIR_LOCAL)"
endif
	@echo "  install        - compile and install the binaries"
ifneq ($(SYSTEM),)
	@echo "                   to $(INSTALLDIR)"
endif
	@echo "  relink         - relink and install the binaries"
ifneq ($(SYSTEM),)
	@echo "                   to $(INSTALLDIR_LOCAL)"
endif
	@echo "  disasm         - disassemble first target"
	@echo "  scrub          - delete backup and temporary files"
	@echo "  clean          - delete generated object files"
	@echo "  cleanall       - delete all generated, backup and temporary files"
	@echo "  help           - this help"
	@echo
ifneq ($(SYSTEM),)
	@echo "  binaries are: $(TARGET)"
else
	@echo "  build for architectures: $(TARGET_SYSTEMS)"
endif

endif	# _L4DIR_MK_PROG_MK undefined

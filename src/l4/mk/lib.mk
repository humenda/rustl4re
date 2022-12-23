# -*- Makefile -*-
#
# DROPS (Dresden Realtime OPerating System) Component
#
# Makefile-Template for library directories
#
# install.inc is used, see there for further documentation
# binary.inc is used, see there for further documentation


ifeq ($(origin _L4DIR_MK_LIB_MK),undefined)
_L4DIR_MK_LIB_MK=y

ROLE = lib.mk

include $(L4DIR)/mk/Makeconf

# define INSTALLDIRs prior to including install.inc, where the install-
# rules are defined. Same for INSTALLDIR.
ifeq ($(MODE),host)
INSTALLDIR_LIB		?= $(DROPS_STDDIR)/lib/host
INSTALLDIR_LIB_LOCAL	?= $(OBJ_BASE)/lib/host
else
INSTALLDIR_LIB		?= $(DROPS_STDDIR)/lib/$(subst -,/,$(SYSTEM))
INSTALLDIR_LIB_LOCAL	?= $(OBJ_BASE)/lib/$(subst -,/,$(SYSTEM))
endif

do_strip=$(and $(CONFIG_BID_STRIP_BINARIES),$(filter %.so,$(1)))
INSTALLFILE_LIB         ?= $(if $(call do_strip,$(1)),                      \
                                $(call copy_stripped_binary,$(1),$(2),644), \
                                $(INSTALL) -m 644 $(1) $(2))
INSTALLFILE_LIB_LOCAL   ?= $(if $(call do_strip,$(1)),                      \
                                $(call copy_stripped_binary,$(1),$(2),644), \
                                $(LN) -sf $(call absfilename,$(1)) $(2))

INSTALLFILE		= $(INSTALLFILE_LIB)
INSTALLDIR		= $(INSTALLDIR_LIB)
INSTALLFILE_LOCAL	= $(INSTALLFILE_LIB_LOCAL)
INSTALLDIR_LOCAL	= $(INSTALLDIR_LIB_LOCAL)

# our mode
MODE			?= lib

# sanity check for proper mode
ifneq ($(filter-out lib host,$(MODE)),)
$(error MODE=$(MODE) not possible when building libraries)
endif

# removed 2014/02
ifneq ($(BUILD_PIC),)
$(error BUILD_PIC=$(BUILD_PIC) is obsolete, use <xxx>.p.a as extra TARGET instead)
endif

# all libraries are built using the wraped utcb-getter
CPPFLAGS          += -DL4SYS_USE_UTCB_WRAP=1

# include all Makeconf.locals, define common rules/variables
include $(L4DIR)/mk/binary.inc
$(GENERAL_D_LOC): $(L4DIR)/mk/lib.mk

ifneq ($(SYSTEM),) # if we are a system, really build

TARGET_LIB        := $(TARGET) $(TARGET_$(OSYSTEM))
TARGET_SHARED     := $(filter     %.so,$(TARGET_LIB))
TARGET_PIC        := $(filter     %.p.a,$(TARGET_LIB))

TARGET_STANDARD   := $(filter-out $(TARGET_SHARED) $(TARGET_PIC), $(TARGET_LIB))

$(call GENERATE_PER_TARGET_RULES,$(TARGET_STANDARD))
$(call GENERATE_PER_TARGET_RULES,$(TARGET_PIC) $(TARGET_SHARED),.s)

TARGET_PROFILE  := $(patsubst %.a,%.pr.a,\
			$(filter $(BUILD_PROFILE),$(TARGET_STANDARD)))
TARGET_PROFILE_SHARED := $(filter %.so,$(TARGET_PROFILE))
TARGET_PROFILE_PIC := $(patsubst %.a,%.p.a,\
			$(filter $(BUILD_PIC),$(TARGET_PROFILE)))

$(call GENERATE_PER_TARGET_RULES,$(TARGET_PROFILE),.pr)
$(call GENERATE_PER_TARGET_RULES,$(TARGET_PROFILE_PIC) $(TARGET_PROFILE_SHARED),.pr)

TARGET	+= $(TARGET_$(OSYSTEM))
TARGET	+= $(TARGET_PROFILE) $(TARGET_PROFILE_SHARED) $(TARGET_PROFILE_PIC)

# define some variables different for lib.mk and prog.mk
LDFLAGS += $(addprefix -L, $(PRIVATE_LIBDIR) $(PRIVATE_LIBDIR_$(OSYSTEM)) $(PRIVATE_LIBDIR_$@) $(PRIVATE_LIBDIR_$@_$(OSYSTEM)))
LDFLAGS += $(addprefix -L, $(L4LIBDIR))
LDFLAGS += $(LIBCLIBDIR)
LDFLAGS_SO ?= -shared

BID_LDFLAGS_FOR_LINKING_DYN_LD  = $(LDFLAGS)
BID_LDFLAGS_FOR_GCC_DYN         = $(filter     -static -shared -nostdlib -Wl$(BID_COMMA)% -L% -l%,$(LDFLAGS))
BID_LDFLAGS_FOR_LD_DYN          = $(filter-out -static -shared -nostdlib -Wl$(BID_COMMA)% -L% -l%,$(LDFLAGS))
BID_LDFLAGS_FOR_LINKING_DYN_GCC = $(addprefix -Wl$(BID_COMMA),$(BID_LDFLAGS_FOR_LD_DYN)) $(BID_LDFLAGS_FOR_GCC_DYN)


LDSCRIPT       = $(LDS_so)
LDSCRIPT_INCR ?= /dev/null

# install.inc eventually defines rules for every target
include $(L4DIR)/mk/install.inc

ifeq ($(NOTARGETSTOINSTALL),)

# non-rust case
ifeq ($(SRC_RS),)
PC_LIBS     ?= $(sort $(patsubst lib%.so,-l%,$(TARGET_SHARED) \
                      $(patsubst lib%.a,-l%,$(TARGET_STANDARD))))
else
# We leave the linking solely to Cargo for our benchmarking to simplify things. This is why we
# filter rlibs.
PC_LIBS     ?= $(filter %.rlib,$(sort $(patsubst lib%.so,-l%,$(TARGET_SHARED) \
                      $(patsubst lib%.a,-l%,$(TARGET_STANDARD)))))
endif

PC_FILENAME  ?= $(PKGNAME)
PC_FILENAMES ?= $(PC_FILENAME)
PC_FILES     := $(foreach pcfile,$(PC_FILENAMES),$(OBJ_BASE)/pc/$(pcfile).pc)

# 1: basename
# 2: pcfilename
get_cont = $(if $($(1)_$(2)),$($(1)_$(2)),$($(1)))

# Ths must contain all the contents of all possible PC files as used in
# below generate_pcfile
PC_FILES_CONTENTS := $(strip $(foreach pcfile,$(PC_FILENAMES),\
  $(call get_cont,CONTRIB_INCDIR,$(pcfile)) \
  $(call get_cont,PC_LIBS,$(pcfile)) \
  $(call get_cont,REQUIRES_LIBS,$(pcfile)) \
  $(call get_cont,PC_CFLAGS,$(pcfile)) $(call get_cont,PC_EXTRA,$(pcfile))))

ifneq ($(PC_FILES_CONTENTS),)

# when adding something to generate_pcfile it must also be added to the
# PC_FILES_CONTENTS above, otherwise PC files may not be generated
$(patsubst %,$(OBJ_BASE)/pc/%.pc,$(PC_FILENAMES)):$(OBJ_BASE)/pc/%.pc: $(GENERAL_D_LOC)
	@$(call GEN_MESSAGE,$(@F))
	$(VERBOSE)$(call generate_pcfile,$*,$@,$(call get_cont,CONTRIB_INCDIR,$*),$(call get_cont,PC_LIBS,$*),$(call get_cont,REQUIRES_LIBS,$*),$(call get_cont,PC_CFLAGS,$*),$(call get_cont,PC_EXTRA,$*))

all:: $(PC_FILES)

endif
endif

DEPS	+= $(foreach file,$(TARGET), $(call BID_LINK_DEPS,$(file)))

$(filter-out $(LINK_INCR) %.so %.o.a %.o.pr.a %.rlib, $(TARGET)):%.a: $(OBJS)
	@$(AR_MESSAGE)
	$(VERBOSE)$(call create_dir,$(@D))
	$(VERBOSE)$(RM) $@
	$(VERBOSE)$(AR) crs$(if $(filter %.thin.a,$@),T) $@ $(OBJS)
	@$(BUILT_MESSAGE)

# shared lib
$(filter %.so, $(TARGET)):%.so: $(OBJS) $(LIBDEPS)
	@$(LINK_SHARED_MESSAGE)
	$(VERBOSE)$(call create_dir,$(@D))
	$(VERBOSE)$(call MAKEDEP,$(LD)) $(BID_LINK) -MD -MF $(call BID_link_deps_file,$@) -o $@ $(LDFLAGS_SO) \
	  $(LDFLAGS) $(OBJS) $(addprefix -PC,$(REQUIRES_LIBS))
	@$(BUILT_MESSAGE)

# build an object file (which looks like a lib to a later link-call), which
# is either later included as a whole or not at all (important for static
# constructors)
LINK_INCR_TARGETS = $(filter $(LINK_INCR) %.o.a %.o.pr.a, $(TARGET))
$(LINK_INCR_TARGETS):%.a: $(OBJS) $(LIBDEPS) $(foreach x,$(LINK_INCR_TARGETS),$(LINK_INCR_ONLYGLOBSYMFILE_$(x)))
	@$(LINK_PARTIAL_MESSAGE)
	$(VERBOSE)$(call create_dir,$(@D))
	$(VERBOSE)$(call MAKEDEP,$(LD)) $(LD) \
	   -T $(LDSCRIPT_INCR) \
	   -o $@ -r $(OBJS) $(LDFLAGS)
	$(if $(LINK_INCR_ONLYGLOBSYM_$@)$(LINK_INCR_ONLYGLOBSYMFILE_$@), \
	   $(VERBOSE)$(OBJCOPY) \
	   $(foreach f,$(LINK_INCR_ONLYGLOBSYMFILE_$@),--keep-global-symbols=$(f)) \
	   $(foreach f,$(LINK_INCR_ONLYGLOBSYM_$@),-G $(f)) \
	   $@)
	@$(BUILT_MESSAGE)

################################################################################
# Rust target

ifneq ($(SRC_RS),)
include $(L4DIR)/mk/cargo.mk

contains=$(findstring $(1),$(strip $(TARGET)))
ifeq ($(and $(call contains,-), $(call contains,lib), $(call contains,.rlib))),)
$(error A library target has to have a name like libFOO-rust.rlib, while FOO is the name of your library and rust a freely choosable suffix required by rustc.)
endif

ifneq ($(DEBUG),)
ifeq ($(filter -g,$(RSFLAGS)),)
RSFLAGS += -g
endif
endif

# Helper variable to keep CARGO_BUILD_RUSTFLAGS readable.
L4_LIBDIRECTORIES=$(filter -L%,$(LDFLAGS))

# The CARGO* variables are exported to instruct Cargo where to find and how to
# use libraries. They are exported to be passed to the subprocess.
# First, add BID defines, replace -D through --cfg and strip =1 (Rust features
# are on or off).
CARGO_BUILD_RUSTFLAGS=$(strip $(patsubst %=1,%,\
		$(patsubst -D%,--cfg=%,$(filter -D%,$(CPPFLAGS)))))
# Add libraries; treat proc macros (run on the host) and normal rust libraries
# (linked into the target binary) differently
CARGO_BUILD_RUSTFLAGS += $(strip $(PROC_MACRO_LIBDIRS) $(L4_LIBDIRECTORIES) \
		$(addprefix -L,$(addsuffix /$(RUST_TARGET),$(_RUST_DEP_PATHS))) \
		$(addprefix -L dependency=,$(addsuffix /$(RUST_TARGET)/deps,$(_RUST_DEP_PATHS))) \
		$(addprefix -L dependency=,$(addsuffix /host-deps,$(_RUST_DEP_PATHS))) \
		$(RSFLAGS))

# This can be used from the build script to get access to all l4-related
# include files
export L4_INCLUDE_DIRS=$(filter -I%,$(CPPFLAGS))

# Create a link in $(OBJ_BASE)/lib/rustlib
# $(1) SRC $(2) DST in $OBJ_BASE/lib/rustlib/PKGNAME
mklink_rustlib=$(VERBOSE)rm -f $(OBJ_BASE)/lib/rustlib/$(PKGNAME)/$(2) \
			   && ln -s $(1) $(OBJ_BASE)/lib/rustlib/$(PKGNAME)/$(2)

$(info PKGDIR=${PKGDIR}, PKGNAME=${PKGNAME})
# Add extra information to each PC file of a procmacro crate.
#ifneq ($(call is_pkg_procmacro),)
#PC_EXTRA += Rust-Cratetype: proc-macro
#endif

$(FS_TARGET_NAME): $(SRC_FILES)
	@echo [$(PKGNAME)] Generating dependency information...
endif # SRC_RS is defined

endif	# architecture is defined, really build

.PHONY: all clean cleanall config help install oldconfig txtconfig
-include $(DEPSVAR)
help::
	@echo "  all            - compile and install the libraries locally"
ifneq ($(SYSTEM),)
	@echo "                   to $(INSTALLDIR_LOCAL)"
endif
	@echo "  install        - compile and install the libraries globally"
ifneq ($(SYSTEM),)
	@echo "                   to $(INSTALLDIR)"
endif
	@echo "  scrub          - delete backup and temporary files"
	@echo "  clean          - delete generated object files"
	@echo "  cleanall       - delete all generated, backup and temporary files"
	@echo "  help           - this help"
	@echo
ifneq ($(SYSTEM),)
	@echo "  libraries are: $(TARGET)"
else
	@echo "  build for architectures: $(TARGET_SYSTEMS)"
endif

endif	# _L4DIR_MK_LIB_MK undefined

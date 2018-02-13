# -*- Makefile -*-
# vim:set ft=make:
#
# DROPS (Dresden Realtime OPerating System) Component
#
# Makefile-Include for compiling templates (prog.mk, lib.mk)

# Makefile-Include for binary and lib directories
# Definitions for building the Makefile.inc, building dependencies,
# compiler configuration.
#
# If SYSTEM is defined and we do not clean, we generate Makefile.inc. This file
# holds the dependencies of the targets. Is also contains the definitions of
# variables holding the objects of the related targets. In a multiple
# architecture makefile, Makefile.inc will be built in arch-specific subdirs, 
# like the other components built.
#
# Most compiler- and linker-flag variables are defined in such a way that
# using them in the rule will expand all the target- and
# architecture-specific definitions.
#
# The relink-rule is defined in this file.
#
# The dependencies for compiling files are defined here, the dep-files are
# generated with names .(source-file).d for .c, .cc and .S-files.
#
# Clean-rules are defined here.
#

$(GENERAL_D_LOC): $(L4DIR)/mk/binary.inc $(L4DIR)/mk/modes.inc $(L4DIR)/mk/rules.inc

SRC_CC_IS_CXX11 ?= y

# our default Makefile-name in the OBJ-Subdirs
BID_OBJ_Makefile ?= Makefile

ifeq ($(SYSTEM),) # if we have no system yet, build the subdirs
#################################################################
#
# we have NO system defined in $(SYSTEM), we are in the src/ dir
#
#################################################################

# our default systems
SYSTEMS	?= $(SYSTEMS_ABI)

DEPENDS_PKGS ?= $(if $(PKG_OPTIONAL),$(REQUIRES_LIBS))

BID_MISSING_PCS := \
   $(strip $(if $(DEPENDS_PKGS), \
             $(strip $(foreach i,$(DEPENDS_PKGS),            \
	               $(if $(strip $(wildcard $(OBJ_BASE)/pc/$(i).pc)),,$(i))))))

ifneq ($(strip $(SRC_F)$(foreach t,$(TARGET),$(SRC_F_$(t)))),)
  ifeq ($(filter x86 amd64,$(BUILD_ARCH)),)
    $(info $(shell echo -e "\033[32mFortran is only available on x86 and amd64 currently, skipping directory '$(SRC_DIR)'.\033[0m"))
    SYSTEMS =
  else
    ifeq ($(GCCFORTRANAVAIL),)
      $(info $(shell echo -e "\033[32mFortran compiler (gcc-4.6 or later) missing, skipping directory '$(SRC_DIR)'.\033[0m"))
      SYSTEMS =
    endif
  endif
endif

ifneq ($(BID_MISSING_PCS),)
  # clear SYSTEMS to prevent building anything
  SYSTEMS =
  text := $(shell echo -e "\033[32mPackage dependencies missing: \033[1m$(BID_MISSING_PCS)\033[22m; skipping directory '$(SRC_DIR)'.\033[0m")
  $(if $(BID_FAIL_ON_MISSING),$(error $(text)), $(info $(text)))
endif

# intersection with BUILD_SYSTEMS

# filter the systems from the local SYSTEMS variable ($2), that match
# the build-architecture.
# args: $(1) - build architecture (one from BUILD_SYSTEMS)
#       $(2) - SYSTEMS
# 1. check, if both systems are the same (optionally reduced by the CPU)
# 2. check, if at least the arch matches (and optionally the cpu)
FILTER_SYSTEM = $(shell echo $(2)|$(AWKP) '\
			BEGIN{m=s="$(1)";sub("_[^-]*","",m)}\
			{for(i=1;i<=NF;i++){\
			 	if(m==$$i||s==$$i){print s}else\
			 	if(index(m,$$i)==1||index(s,$$i)==1)\
			 		{t=s;sub("-.*","",t);print t}}}')

# print that system of the SYSTEMS variable that actually matched with
# $(BUILD_SYSTEMS) to the given system
# args: $(1) - build architecture (SYSTEM)
#       $(2) - SYSTEMS
# + do nearly the same as in FILTER_SYSTEM, but additionally check if
#   the resulting system matches $(1). If so, print the according pattern from
#   SYSTEMS and exit
BID_ORIG_SYSTEM = $(shell echo $(2)|$(AWKP) '\
		    BEGIN{m=s="$(1)";sub("_[^-]*","",m)}\
		    {for(i=1;i<=NF;i++){\
		 	if(m==$$i||s==$$i){print $$i;exit};\
		 	if(index(m,$$i)==1||index(s,$$i)==1)\
			    {t=s;sub("-.*","",t);\
                             if(m==$$i&&t=="$(1)"){print $$i;exit}}}}')

ifeq ($(ROLE),idl.mk)
TARGET_SYSTEMS := $(sort $(foreach sys,$(sort \
	$(shell echo $(BUILD_SYSTEMS)|$(SED) -e 's/_[^- ]*\(-\{0,1\}\)/\1/g')), \
	$(call FILTER_SYSTEM,$(sys),$(SYSTEMS))))
else
TARGET_SYSTEMS := $(sort $(foreach sys,$(BUILD_SYSTEMS),\
			$(call FILTER_SYSTEM,$(sys),$(SYSTEMS))))
endif

DIR_FROM_SUB = $(firstword $(addprefix ../,$(patsubst /%,,$(1))) $(1))
SYSTEM_TO_ARCH = $(shell echo $(1)|$(SED) -e 's/[_-].*//')
SYSTEM_TO_CPU = $(shell echo $(1)|$(SED) -ne 's/[^-_]*_\([^-]*\).*/\1/p')
SYSTEM_TO_L4API = $(shell echo $(1)|$(SED) -ne 's/.*-\(.*\)/\1/p')

$(foreach sys,$(TARGET_SYSTEMS),$(OBJ_DIR)/OBJ-$(sys)/$(BID_OBJ_Makefile)):$(OBJ_DIR)/OBJ-%/$(BID_OBJ_Makefile):$(OBJ_DIR)/.general.d $(L4DIR_ABS)/mk/binary.inc
	@install -d $(dir $@)
	@echo 'L4DIR=$(L4DIR_ABS)'>$@
	@echo 'OBJ_BASE=$(OBJ_BASE)'>>$@
	@echo 'OBJ_DIR=$(OBJ_DIR)'>>$@
	@echo 'SRC_DIR=$(SRC_DIR)'>>$@
	@echo 'PKGDIR=$(PKGDIR_ABS)'>>$@
	@echo 'PKGDIR_ABS=$(PKGDIR_ABS)'>>$@
	@echo 'PKGDIR_OBJ=$(PKGDIR_OBJ)'>>$@
	@echo 'MAKECONFLOCAL=$(SRC_DIR)/Makeconf.local'>>$@
	@echo 'OSYSTEM=$(call BID_ORIG_SYSTEM,$*,$(SYSTEMS))'>>$@
	@echo 'SYSTEM=$*'>>$@
	@echo 'ARCH=$(call SYSTEM_TO_ARCH,$*)'>>$@
	@echo 'CPU=$(call SYSTEM_TO_CPU,$*)'>>$@
	@echo 'L4API=$(call SYSTEM_TO_L4API,$*)'>>$@
	@for ext in .c .cc .cpp $(FORTRAN_FILE_EXTENSIONS) $(RUST_FILE_EXTENSIONS) $(BID_ASM_FILE_EXTENSIONS) .y .l .ld .dpe .dts; \
	  do echo "vpath %$$ext $(VPATH_SRC_BASE)">>$@ ; done
	@echo '.general.d: $(SRC_DIR)/$(if $(wildcard Make.rules),Make.rules,Makefile)'>>$@
	@echo 'include $(SRC_DIR)/$(if $(wildcard Make.rules),Make.rules,Makefile)'>>$@
	@echo 'include $$(L4DIR)/mk/$(ROLE)'>>$@

install relink scrub:: $(foreach arch,$(TARGET_SYSTEMS),\
                                          $(OBJ_DIR)/OBJ-$(arch)/$(BID_OBJ_Makefile))
	$(if $^,\
	    $(VERBOSE)set -e; \
	    for d in $(^D); do \
	        cd $$d; $(MAKE) -f $(BID_OBJ_Makefile) $@; \
	    done )

.PHONY: pre-obj
pre-obj::

all:: $(foreach arch,$(TARGET_SYSTEMS), $(OBJ_DIR)/OBJ-$(arch))
.PHONY: $(foreach arch,$(TARGET_SYSTEMS), $(OBJ_DIR)/OBJ-$(arch))
$(foreach arch,$(TARGET_SYSTEMS), $(OBJ_DIR)/OBJ-$(arch)):%:%/$(BID_OBJ_Makefile) pre-obj
	$(VERBOSE)PWD=$@ $(MAKE) $(PL_j) -C $@ -f $(BID_OBJ_Makefile)


foreach_objdir = $(if $(wildcard $(OBJ_DIR)/OBJ-*), $(VERBOSE)set -e ; \
		     for d in $(wildcard $(OBJ_DIR)/OBJ-*) ; do \
		       PWD=$$d $(MAKE) -C $$d -f $(BID_OBJ_Makefile) $(1); \
		     done, @true)

%.i %.s.i:: export DO_SHOW_RESULT_FILE=y
%.i %.s.i::
	$(call foreach_objdir,$@)

clean disasm::
	$(call foreach_objdir,$@)

cleanall::
	$(VERBOSE)$(RM) -r $(wildcard $(OBJ_DIR))

.PHONY: $(TARGET_SYSTEMS)

else
###############################################################
#
# we have a system defined in $(SYSTEM), we are in an OBJ- dir
#
###############################################################

all::		$(TARGET)

disasm: $(TARGET)
	$(call DISASM_CMD,$(if $(DABIN),$(DABIN),$<))

ifneq ($(CONFIG_USE_DROPS_STDDIR),)
L4INCDIR ?=	$(addprefix $(OBJ_BASE)/include/$(ARCH)/,$(L4API)) \
                $(addprefix $(OBJ_BASE)/include/,$(L4API)) \
                $(OBJ_BASE)/include/$(ARCH) \
                $(OBJ_BASE)/include \
		$(if $(USE_DICE),$(DICE_INCDIR),) \
                $(addprefix $(DROPS_STDDIR)/include/$(ARCH)/,$(L4API)) \
                $(addprefix $(DROPS_STDDIR)/include/,$(L4API)) \
                $(DROPS_STDDIR)/include/$(ARCH) \
                $(DROPS_STDDIR)/include

L4LIBDIR ?=	$(addprefix $(OBJ_BASE)/lib/$(ARCH)_$(CPU)/,$(L4API)) \
                $(OBJ_BASE)/lib/$(ARCH)_$(CPU) \
                $(OBJ_BASE)/lib \
                $(addprefix $(DROPS_STDDIR)/lib/$(ARCH)_$(CPU)/,$(L4API)) \
                $(DROPS_STDDIR)/lib/$(ARCH)_$(CPU) \
                $(DROPS_STDDIR)/lib

else

L4INCDIR ?=	$(addprefix $(OBJ_BASE)/include/$(ARCH)/,$(L4API)) \
                $(wildcard $(addprefix $(OBJ_BASE)/include/,$(L4API))) \
                $(OBJ_BASE)/include/$(ARCH) \
                $(OBJ_BASE)/include \
		$(if $(USE_DICE),$(DICE_INCDIR),)

L4LIBDIR ?=     $(addprefix $(OBJ_BASE)/lib/$(ARCH)_$(CPU)/,$(L4API)) \
                $(OBJ_BASE)/lib/$(ARCH)_$(CPU) \
                $(OBJ_BASE)/lib
endif

#
# Variables Section
#
# There is a hierarchy on defining variables depending on the targets they
# refer to: Most standard-Make Variables are supported. This includes
# LDFLAGS	- options for ld, defined in prog.mk and lib.mk
# CPPFLAGS	- options for the c preprocessor, included in CFLAGS
# CFLAGS	- options for the c compiler
# CXXFLAGS	- options for the c++ compiler
# FFLAGS	- options for the fortran compiler
# ASFLAGS	- options for the assembler
# RSFLAGS   - options for the Rustc compiler
#
# Additionally, the following variables are supported:
# SRC_C, SRC_CC, SRC_F, SRC_S, SRC_RS - .c, .cc, .f90, .S, .rs source files
# LIBS		- additional libs to link (with -l), including paths (-L)
# IDL		- interface definition files
# TARGET	- targets to be built
#
# These variables will be used for all operations with the corresponding
# file types. More specific description is possible by using variables with
# added specifications. These specifications include a referred element and
# the architecture, both optional but in this order, separated by
# underscores. The referred element for CPPFLAGS, CFLAGS, CXXFLAGS, ASFLAGS and
# RSFLAGS is the source file. For the other variables, it is one of the
# target files. The TARGET variable can only be postfixed by an
# architecture.
# The specific variables will be used for the target and the referred element
# given in the name, additionally to the more general ones.
#
# Example for a valid specifications:
# SRC_C_libxverbose.a = verbose.c   - ar's verbose.o into libxverbose.a, but
#                                     not in other libs in the TARGET var.

include $(L4DIR)/mk/modes.inc

# select the variable specified in $(1) from the current architecture and
# mode. Fall back to "all" architecture if no specific version exists.
BID_mode_var= $(if $($(1)_$(ARCH)_$(MODE)),$($(1)_$(ARCH)_$(MODE)),$($(1)_all_$(MODE)))

BID_SUPPORTED ?= $(call BID_mode_var,BID_SUPPORTED)
ifneq ($(BID_SUPPORTED),y)
$(error Mode "$(MODE)" is not supported for CPU architecture "$(ARCH)")
endif

LIBCINCDIR    ?= $(call BID_mode_var,LIBCINCDIR)
LIBCLIBDIR    ?= $(call BID_mode_var,LIBCLIBDIR)
LDSCRIPT      ?= $(call BID_mode_var,LDSCRIPT)
LDFLAGS       += $(call BID_mode_var,LDFLAGS)
REQUIRES_LIBS += $(REQUIRES_LIBS_$(ARCH)) $(call BID_mode_var,REQUIRES_LIBS)

LDFLAGS	+= $(strip $(LDFLAGS_$(OSYSTEM)) $(LDFLAGS_$@) $(LDFLAGS_$@_$(OSYSTEM)))
CARCHFLAGS += $(CARCHFLAGS_$(ARCH)) $(CARCHFLAGS_$(ARCH)_$(CPU))

CFLAGS        += $(CFLAGS-y)
CXXFLAGS      += $(CXXFLAGS-y)
CPPFLAGS      += $(CPPFLAGS-y)
SRC_C         += $(SRC_C-y)
SRC_CC        += $(SRC_CC-y)
SRC_S         += $(SRC_S-y)
DEFINES       += $(DEFINES-y)
REQUIRES_LIBS += $(REQUIRES_LIBS-y)

# ---------------------------------

BID_MISSING_LIBS :=

# call pkg-config, returns __PKGCONFIG_FAILED__ if the call failed
# 1: OBJ_BASE
# 2: parameters to pkg-config
# 3: list of packages
# use L4_BID_PKG_CONFIG because of export-defs, the function is copied
L4_BID_PKG_CONFIG = $(L4DIR)/tool/bin/l4-bender
BID_PKG_CONFIG =                                      \
                   $(shell PKG_CONFIG_LIBDIR=$(1)/pc                     \
                           PKG_CONFIG_PATH= $(L4_BID_PKG_CONFIG)         \
                           --define-variable=incdir=$(1)/include/contrib \
                           $(if $(VERBOSE),--silence-errors)             \
			   $(2) $(3) || echo __PKGCONFIG_FAILED__)

BID_PKG_CONFIG_MISSING =                                         \
     $(if $(strip $(3)),                                         \
          $(shell PKG_CONFIG_LIBDIR=$(1)/pc                      \
                  PKG_CONFIG_PATH= LC_ALL=C $(L4_BID_PKG_CONFIG) \
                  --errors-to-stdout --print-errors $(2) $(3)    \
                  | LC_ALL=C grep ", not found"))

BID_PKG_CONFIG_FAILED = $(findstring __PKGCONFIG_FAILED__,$(1))

BID_BENDER_DEFINES = \
	-Dl4obj=$(1)                  \
	-Dl4dir=$(2)                  \
	-Dgcclibdir="$(3:-L%=%)"      \
	-Dl4system=$(4)               \
	$(if $(5),-Dl4api=$(5))

bid_bender_default_vars = $(call BID_BENDER_DEFINES,$(OBJ_BASE),$(L4DIR),$(GCCSYSLIBDIRS),$(ARCH)_$(CPU),$(L4API))

# linker for L4 libs and applications
BID_LINK = $(L4DIR)/tool/bin/l4-bender $(if $(VERBOSE),,--trace-exec) \
           -t ld $(bid_bender_default_vars) -Dlinker="$(LD)" \
           --spec=$(L4DIR)/mk/bid-bender.spec --

# linker for host mode and l4linux mode
BID_LINK_MODE_host = $(L4DIR)/tool/bin/l4-bender $(if $(VERBOSE),,--trace-exec) \
                     -t host-ld $(bid_bender_default_vars) -Dlinker="$(1)" \
                     --spec=$(L4DIR)/mk/bid-bender.spec --

BID_link_deps_file = $(call BID_dot_fname,$1).pcs.d

BID_LINK_DEPS = $(call BID_dot_fname,$1).d $(call BID_link_deps_file,$1)

ifeq ($(BID_MISSING_PCS),)
 ifneq ($(SYSTEM),)
  ifneq ($(REQUIRES_LIBS),)
   REQUIRES_LIBS_LIST := $(strip $(call BID_PKG_CONFIG,$(OBJ_BASE),--libs,$(REQUIRES_LIBS)))
   # error handling
   ifneq ($(call BID_PKG_CONFIG_FAILED,$(REQUIRES_LIBS_LIST)),)
    BID_MISSING_PCS := $(strip $(foreach i,$(REQUIRES_LIBS), \
                         $(if $(filter __PKGCONFIG_FAILED__,$(call BID_PKG_CONFIG,$(OBJ_BASE),--libs --print-errors,$(i))),$(i))))
    text := $(shell echo -e "\033[31mLibrary dependencies missing: \033[1m$(BID_MISSING_PCS)\033[22m in directory '$(SRC_DIR)'; aborting.\033[0m")
    text2 := $(strip $(foreach i,$(REQUIRES_LIBS), $(call BID_PKG_CONFIG_MISSING,$(OBJ_BASE),--libs,$(i))))
    $(if $(text2),$(info $(shell echo -e "\033[31m$(text2)\033[0m")))
    $(if $(BID_MISSING_PCS),$(info $(text)),$(error $(text)))
   endif

   # some sanity check
   ifneq ($(filter libl4re-vfs,$(REQUIRES_LIBS)),)
     $(error Never include 'libl4re-vfs'!)
   endif
  endif

  BID_PKG_CONFIG_CFLAGS := $(call BID_PKG_CONFIG,$(OBJ_BASE),--cflags, $(REQUIRES_CFLAGS) $(REQUIRES_LIBS))

  # error handling
  ifneq ($(call BID_PKG_CONFIG_FAILED,$(BID_PKG_CONFIG_CFLAGS)),)
   BID_MISSING_PCS := $(strip $(foreach i,$(REQUIRES_CFLAGS) $(REQUIRES_LIBS), \
                          $(if $(filter __PKGCONFIG_FAILED__,$(call BID_PKG_CONFIG,$(OBJ_BASE),--cflags --print-errors,$(i))),$(i))))
   text := $(shell echo -e "\033[31mCflags dependencies missing: \033[1m$(BID_MISSING_PCS)\033[22m in directory '$(SRC_DIR)'; aborting.\033[0m")
   text2 := $(strip $(foreach i,$(REQUIRES_LIBS), $(call BID_PKG_CONFIG_MISSING,$(OBJ_BASE),--cflags,$(i))))
   $(if $(text2),$(info $(shell echo -e "\033[31m$(text2)\033[0m")))
   $(error $(text))
  endif
 endif # SYSTEM
endif

ifneq ($(REQUIRE_HOST_TOOLS),)
  CHECK_HOST_TOOLS = \
     $(shell unset mis;                                     \
	     for i in $(1); do                              \
	       if ! command -v $$i >/dev/null 2>&1; then    \
	         [ -n "$$mis" ] && mis="$$mis ";            \
	         mis="$$mis$$i";                            \
	       fi                                           \
	     done; echo $$mis)

  ifneq ($(call CHECK_HOST_TOOLS,$(REQUIRE_HOST_TOOLS)),)
    $(info $(shell echo -e "\033[32mHost tool(s) missing: \033[1m$(REQUIRE_HOST_TOOLS)\033[22m needed in directory '$(SRC_DIR)'. Skipping.\033[0m"))
    SYSTEMS :=
    INSTALL_TARGET :=
  endif
endif

# -----------------------

# May 2007: force somewhat old hash-style as some(!, independent of the
#           actual version) ld now use an other format, no uclibc upstream fix
#           available yet, until available, we need:
ifeq ($(LD_HAS_HASH_STYLE_OPTION),y)
ifneq ($(HOST_LINK),1)
LDFLAGS += --hash-style=sysv
endif
endif

bid_flag_variants = $($(1)_$(OSYSTEM)) $($(1)_$@) $($(1)_$(@D)) $($(1)_$<) \
                    $($(1)_$(@D)/$(<F)) \
                    $($(1)_$(<F)) $($(1)_$(<D)) $($(1)_$(<F)_$(OSYSTEM))

OPTS_DEBUG-$(CONFIG_BID_DEBUG_INFO) = -g
OPTS_DEBUG       ?= $(OPTS_DEBUG-y)
OPTS             ?= $(OPTS_DEBUG) -O2 -fno-strict-aliasing

WARNINGS_MINIMAL ?= -Wall $(call bid_flag_variants,WARNINGS)
WARNINGS_MEDIUM  ?= -Wall -Wstrict-prototypes $(CONDITIONAL_WARNINGS_MEDIUM) \
                    -Wmissing-declarations $(call bid_flag_variants,WARNINGS)
WARNINGS_FULL    ?= -Wextra -Wbad-function-cast -Wdouble-promotion \
                    $(CONDITIONAL_WARNINGS_FULL) $(WARNINGS_MEDIUM)
WARNINGS         ?= $(WARNINGS_FULL)

ifeq ($(MODE),host)
# never cross compile in host mode
override CROSS_COMPILE = 
CC            = $(HOST_CC)
CXX           = $(HOST_CXX)
else
# no architecture specific flags in host mode
LDFLAGS      += $(LDFLAGS_$(ARCH))
endif

# additional libraries go after the requires-libs-list
LIBS         += $(strip $(LIBS_$(OSYSTEM)) $(LIBS_$@) $(LIBS_$@_$(OSYSTEM)))

# no link address if build a lib or we're compiling a host program and using
# the host linker scripts
ifeq ($(ROLE),lib.mk)
NO_DEFAULT_RELOC := y
endif
ifneq ($(HOST_LINK),)
NO_DEFAULT_RELOC := y
endif

# stack protector support
# check if uclibc signaled support for stack protector
ifneq ($(filter bid_allow_stack_protector, $(BID_PKG_CONFIG_CFLAGS)),)
  BID_PKG_CONFIG_CFLAGS := $(filter-out bid_allow_stack_protector, $(BID_PKG_CONFIG_CFLAGS))
  ifdef CONFIG_BID_GCC_ENABLE_STACK_PROTECTOR # stack protector option was selected in make config
    GCCSTACKPROTECTOROPT := $(if $(CONFIG_BID_GCC_STACK_PROTECTOR_ALL), $(GCCSTACKPROTALLOPT), \
                                                                        $(GCCSTACKPROTOPT))
    ifneq ('$(CONFIG_BID_GCC_STACK_PROTECTOR_ALL)$(CONFIG_BID_GCC_STACK_PROTECTOR)','y')
      $(error invalid combination of CONFIG_BID_GCC_STACK_PROTECTOR... options)
    endif
  endif # CONFIG_BID_GCC_ENABLE_STACK_PROTECTOR
else
  # explicity disable the stack protector (some compilers enable stack protector by default)
  GCCSTACKPROTECTOROPT = $(GCCNOSTACKPROTOPT)
endif # stack protector

default_reloc_base = $(firstword $(DEFAULT_RELOC_$(1)) \
                                 $(DEFAULT_RELOC_$(ARCH)) \
                                 $(DEFAULT_RELOC) \
                                 $(if $(NO_DEFAULT_RELOC),,0x01000000))
default_reloc = $(if $(call default_reloc_base,$(1)),\
                     $(if $(RELOC_PHYS), \
		          $(shell printf "0x%x" $$(($(RAM_BASE) + $(call default_reloc_base,$(1))))), \
			  $(call default_reloc_base,$(1))))

CCXX_FLAGS += $(if $(CONFIG_FULL_PATH_NAMES_IN_BINARIES),,$(GCCPREFIXOPT))

# can be overwritten to get the old mode
CFLAGS_C99 ?= -std=gnu99

ifneq ($(MODE),host)
CFLAGS_L4_GENERIC += $(CCXX_FLAGS) $(CARCHFLAGS) $(GCCSTACKPROTECTOROPT)
endif


$(CLIENTIDL:.idl=-client.o): CFLAGS += -ffunction-sections
CFLAGS  += $(if $(CONFIG_BID_GCC_OMIT_FP),-fomit-frame-pointer,-fno-omit-frame-pointer)
CFLAGS	+= $(call bid_flag_variants,CFLAGS)
CFLAGS	+= $(OPTS) $(WARNINGS)
CFLAGS  += -fno-common
CFLAGS  += $(CFLAGS_C99)
CFLAGS  += $(CFLAGS_L4_GENERIC)
DEFINES += $(call bid_flag_variants,DEFINES)
DEFINES += -DSYSTEM_$(subst -,_,$(SYSTEM)) -DARCH_$(ARCH) -DCPUTYPE_$(CPU) -DL4API_$(L4API)
DEFINES += -D_GNU_SOURCE $(if $(L4API),,-DL4_MINIMAL_LIBC)
ifdef DEBUG
ifneq (,$(filter 0 n N, $(DEBUG)))
DEFINES	+= -DNDEBUG
endif
endif

CPPFLAGS-DEBUG_MODE = \
    $(if $(L4API), \
      $(addprefix -include ,$(OBJ_BASE)/include/l4/sys/kdebug.h \
                            $(OBJ_BASE)/include/l4/sys/ktrace.h \
                            $(OBJ_BASE)/include/l4/util/kprintf.h \
                            $(OBJ_BASE)/include/l4/sys/debugger.h))

CPPFLAGS+= $(call bid_flag_variants,CPPFLAGS)
CPPFLAGS+= $(DEFINES)
CPPFLAGS+= $(addprefix -I,  $(PRIVATE_INCDIR) $(call bid_flag_variants,PRIVATE_INCDIR))
CPPFLAGS+= $(if $(CONTRIB_INCDIR),$(addprefix -I$(OBJ_BASE)/include/contrib/,$(CONTRIB_INCDIR)))
CPPFLAGS+= $(BID_PKG_CONFIG_CFLAGS)
#CPPFLAGS+= $(if $(CLIENTIDL)$(SERVERIDL)$(addprefix CLIENTIDL_,$(TARGET))$(addprefix SERVERIDL_,$(TARGET)),$(addprefix -I, $(IDL_PATH)))
ifneq ($(MODE),host)
CPPFLAGS+= $(addprefix -I, $(L4INCDIR))
endif
CPPFLAGS+= $(if $(DEBUG_MODE),$(CPPFLAGS-DEBUG_MODE))
CPPFLAGS+= $(LIBCINCDIR)

ifneq ($(MODE),host)
CXXFLAGS_L4_GENERIC += $(CARCHFLAGS) $(CCXX_FLAGS)
CXXFLAGS_L4_GENERIC += $(if $(GCC_HAS_ATOMICS),-DL4_GCC_HAS_ATOMICS)
CXXFLAGS_L4_GENERIC += $(GCCSTACKPROTECTOROPT)
CXXFLAGS_L4_GENERIC += -fuse-cxa-atexit
endif

CXXFLAGS_CXX11 = -std=gnu++11

ifneq ($(SRC_CC_IS_CXX11),)
  CXXFLAGS+= $(CXXFLAGS_CXX11)
endif

CXXFLAGS+= $(if $(CONFIG_BID_GCC_OMIT_FP),-fomit-frame-pointer,-fno-omit-frame-pointer)
CXXFLAGS+= $(call bid_flag_variants,CXXFLAGS)
CXXFLAGS+= $(OPTS) $(filter-out $(foreach w,-Wbad-function-cast -Wstrict-prototypes -Wmissing-prototypes,\
                                            $(w) $(patsubst -W%,-Wno-%,$(w))),\
                                $(WARNINGS))
CXXFLAGS+= $(GCCWNONOEXCEPTTYPE) $(GCCWNOPSABI)
CXXFLAGS+= -fno-common
CXXFLAGS+= $(CXXFLAGS_L4_GENERIC)

FFLAGS  += $(OPTS)
FFLAGS  += $(call bid_flag_variants,FFLAGS)
FFLAGS  += $(CFLAGS_L4_GENERIC)

PICFLAGS += -fPIC -U__PIC__ -D__PIC__=1

# select NOPIEFLAGS and NOPICFLAGS, use the most specific mode variable
# first and the try the more generic _all_ and global version
NOPICFLAGS_all_$(MODE)     ?= $(NOPICFLAGS)
NOPICFLAGS_$(ARCH)_$(MODE) ?= $(NOPICFLAGS_all_$(MODE))

NOPIEFLAGS_all_$(MODE)     ?= $(NOPIEFLAGS)
NOPIEFLAGS_$(ARCH)_$(MODE) ?= $(NOPIEFLAGS_all_$(MODE))

ifneq ($(MODE),host)
ASFLAGS_L4_GENERIC += $(CARCHFLAGS) $(CCXX_FLAGS)
endif

ASFLAGS += $(OPTS) $(ASFLAGS_L4_GENERIC)
ASFLAGS += $(ASFLAGS_$(ARCH)) $(ASFLAGS_$(@:.o=.S)) $(ASFLAGS_$(@:.o=.S)_$(OSYSTEM))

ALLOBJS  = $(OBJS) $(foreach target,$(TARGET) $(TARGET_$(OSYSTEM)),\
                             $(OBJS_$(target)) $(OBJS_$(target)_$(OSYSTEM)))
ALLDPI = $(sort $(foreach obj,$(ALLOBJS),$(patsubst %.dpe,%.dpi,\
		$(DPE_$(obj:.o=.c)))))

# implementation of $(CLIENTIDL) and $(SERVERIDL)
IDL_TYPE   ?= dice
IDL_PKGDIR ?= $(PKGDIR_OBJ)
IDL_PATH   ?= $(addsuffix /idl/OBJ-$(ARCH)-$(L4API),$(IDL_PKGDIR))
ifneq ($(IDL_TYPE), dice)
ifneq ($(IDL_TYPE), corba)
$(error IDL_TYPE "$(IDL_TYPE)" not supported.)
endif
endif

# A function to deliver the source files generated from IDL-files and
# that should be linked to a given target. Used by "targetobjs" to write
# the target-objects and in the "Makefile.inc:"-rule to write the IDL_SRC_C-
# variables
# arg: 1 - target
IDL_SRC_Cfn = $(CLIENTIDL:.idl=-client.c) $(SERVERIDL:.idl=-server.c)	\
	      $(foreach inter, $(OSYSTEM) $(1) $(1)_$(OSYSTEM),		\
			$(CLIENTIDL_$(inter):.idl=-client.c)		\
			$(SERVERIDL_$(inter):.idl=-server.c))
IDL_SRC_CCfn = $(CLIENTIDL:.idl=-client.cc) $(SERVERIDL:.idl=-server.cc) \
	      $(foreach inter, $(OSYSTEM) $(1) $(1)_$(OSYSTEM),		\
			$(CLIENTIDL_$(inter):.idl=-client.cc)		\
			$(SERVERIDL_$(inter):.idl=-server.cc))

# *all* CLIENTIDLs/SERVERIDLs
# needed for the test if we should include the IDL-Path into vpath
IDL_ALLCLIENTIDL = $(CLIENTIDL) $(CLIENTIDL_$(OSYSTEM))			\
		   $(foreach target,$(TARGET) $(TARGET_$(OSYSTEM)),	\
		      $(CLIENTIDL_$(target)) $(CLIENTIDL_$(target)_$(OSYSTEM)))
IDL_ALLSERVERIDL = $(SERVERIDL) $(SERVERIDL_$(OSYSTEM)) 		\
		   $(foreach target,$(TARGET) $(TARGET_$(OSYSTEM)),	\
		      $(SERVERIDL_$(target)) $(SERVERIDL_$(target)_$(OSYSTEM)))

ifneq ($(IDL_ALLCLIENTIDL),)
vpath %-client.c $(IDL_PATH)
vpath %-client.cc $(IDL_PATH)
endif
ifneq ($(IDL_ALLSERVERIDL),)
vpath %-server.c $(IDL_PATH)
vpath %-server.cc $(IDL_PATH)
endif

# adjust obj path in case an absolute path name is given,
# for that to work we also need to explicitly list .o to .source relations
# arg: 1 - file path
gen_obj_path = $(foreach f,$(1),$(if $(filter /%,$(f)),$(OBJ_BASE)$(patsubst $(SRC_BASE)/%,/%,$(f)),$(f)))

# check for absolute paths
# arg: 1 - list of paths
check_path = $(foreach f,$(1),$(if $(filter /%,$(f)),$(error Given paths must be relative.),$(f)))

# convert list of c++ files names to .o files
# arg: 1 - list of files
#      2 - infix between the basename and the .o
convert_cc_to_o_file = $(patsubst %.cc,%$(2).o, $(filter %.cc,$(1)))     \
                       $(if $(BID_ALLOW_CPP_AS_CPLUSPLUS_FILE),          \
		         $(patsubst %.cpp,%$(2).o,$(filter %.cpp,$(1))))

#
# Get the combined contents of generic and target-specific variables.
#
# Returns:
#  $(<var>) $(<var>_$(OSYSTEM)) $(<var>_<target>) $(<var>_<target>_$(OSYSTEM))
#
# arg: 1 - name of the variable (<var>), e.g. SRC_C
#      2 - target name
#
get_target_var = $(strip                                     \
                   $($(1))                                   \
                   $(if $(OSYSTEM),$($(1)_$(OSYSTEM)))       \
                   $($(1)_$(2))                              \
                   $(if $(OSYSTEM),$($(1)_$(2)_$(OSYSTEM))))

# convert list of Fortran file names to .o files
# arg: 1 - list of files
#      2 - infix between the basename and the .o
convert_f_to_o_file = $(foreach e,$(FORTRAN_FILE_EXTENSIONS),$(patsubst %$e,%$(2).o,$(filter %$e,$(1))))

# convert .rs to .o, see fortran doc above
convert_rs_to_o_file = $(foreach e,$(RUST_FILE_EXTENSIONS),$(patsubst %$e,%$(2).o,$(filter %$e,$(1))))

convert_asm_to_o_file = $(foreach e,$(BID_ASM_FILE_EXTENSIONS),$(patsubst %$e,%$(2).o,$(filter %$e,$(1))))

#
# generic source to object file name converters
# args: 1 - (list of) source file(s)
#       2 - object file infix 1 (after the source base-name)
#
gen_SRC_CC_obj = $(call convert_cc_to_o_file,$(1),$(2))
gen_SRC_C_obj  = $(1:.c=$(2).o)
gen_SRC_S_obj  = $(call convert_asm_to_o_file,$(1),$(2))
gen_SRC_F_obj  = $(call convert_f_to_o_file,$(1),$(2))
gen_SRC_RS_obj  = $(call convert_rs_to_o_file,$(1),$(2))
gen_SRC_DATA_obj = $(addsuffix .bin.o,$1)

#
# Check source file constraints and generate and add the object file
# name to OBJS_<target>.
#
# Generates:
#   BID_OBJ_SRC_<object-name> := <source-name>
#   BID_SRC_OBJ_<source-name> += <object-name>
#   OBJS_<target>             += <object-name>
#
# Checks:
# * source files must be relative paths
# * there must be only one source file generating an object file
#
# arg: 1 - target name
#      2 - object file name
#      3 - source file name
#
define make_per_obj_vars
  # $$(info make_per_obj_vars: '$(1)' '$(2)' '$(3)')
  $$(if $$(filter /%,$(3)), $$(error Source file must be relative: $(3)))
  $$(if $$(and $$(BID_OBJ_SRC_$(2)), $$(filter-out $$(BID_OBJ_SRC_$(2)), $(3))), \
    $$(error $(2) has ambigous source files: $(3) $$(BID_OBJ_SRC_$(2))))
  BID_OBJ_SRC_$(2) := $(3)
  BID_SRC_OBJ_$(3) += $(2)
  OBJS_$(1)        += $(2)
endef

#
# Make variables for all source files in $($(2))
#
# args: 1 - target
#       2 - source var name (e.g., SRC_C)
#       3 - object name infix 1
#
define make_per_source_type_vars
  $(foreach src,$(call get_target_var,$(1),$(2)),\
    $(eval $(call make_per_obj_vars,$(2),$(strip $(call gen_$(1)_obj,$(src),$(3))),$(src))))
endef

#
# All supported SRC_<x> variables
#
BID_SRC_VARS ?= S C CC F RS DATA

#
# Generate per target/source/object variables for all sources
# found in SRC_[$(BID_SRC_VARS)].
#
# args: 1 - target
#       2 - object file infix 1
#
define make_per_target_vars
  $(foreach svar,$(BID_SRC_VARS),$(call make_per_source_type_vars,SRC_$(svar),$(1),$(2)))
endef

# function to to use with $(eval ...) that generates per-target
# SRC_C and SRC_CC entries for IDL-compiler generated source files
# NOTE: no IDL compiler is currently used/supported so this function
#       is not used.
define per_target_idl_vars
  IDL_SRC_C_$(1)  = $$(call IDL_SRC_Cfn,$(1))
  SRC_C_$(1)     += $$(IDL_SRC_C_$(1))
  IDL_SRC_CC_$(1) = $$(call IDL_SRC_CCfn,$(1))
  SRC_CC_$(1)    += $$(IDL_SRC_CC_$(1))
endef

#
# Generate all target / object / source specific variables and
# dependencies for a given target.
#
# args: 1 - target name
#       2 - object-file infix 1 (added after the source files base-name)
#
define make_per_target_vars_and_deps
  $$(call make_per_target_vars,$(1),$(2))
  OBJS_$(1) := $$(call get_target_var,OBJS,$(1))
  $$(OBJS_$(1)): .general.d
  $(1): OBJS = $$(OBJS_$(1))
  $(1): $$(OBJS_$(1))
endef

# handle per-target DEFAULT_RELOC.
#
# Create a dependency to $(BID_RAM_BASE_DEP) and LDFLAGS for the given target.
# NOTE: the handling depends on the global MODE setting in the Makefile.
ifneq ($(MODE),shared)
  ifneq ($(filter l4linux host,$(MODE)),)
    define make_per_target_reloc
      $$(error DEFAULT_RELOC / DEFAULT_RELOC_$(1) must not be set if MODE is l4linux or host)
    endef
  else
    define make_per_target_reloc
      $(1): $$(BID_RAM_BASE_DEP)
      LDFLAGS_$(1) += -Ttext-segment=$$(firstword $$(call default_reloc,$(1)))
    endef
  endif
else
  define make_per_target_reloc
  endef
endif

#
# Generate $(1): %.dpi -> %.dpe dependencies for one source file
# arg: 1 - name of the C or C++ source file
#
# This function generates a dependency if DPE_$(1) is not empty
#
define gen_src_to_dpe_deps
  $(if $(DPE_$(1)),$(eval $(SRC_DIR)/$(1): $(patsubst %.dpe,%.dpi,$(DPE_$(1)))))
endef

define gen_target_vars_and_deps
  $(eval $(call make_per_target_vars_and_deps,$(1),$(2)))    \
  $(foreach src,$(call get_target_var,SRC_C,$(1))            \
                $(call get_target_var,SRC_CC,$(1)),          \
                $(call gen_src_to_dpe_deps,$(src)))          \
  $(if $(call default_reloc,$(1)),$(eval $(call make_per_target_reloc,$(1))))
endef

# Generate dependency rules and target-specific variables for all
# targets, using the functions above.
define GENERATE_PER_TARGET_RULES
  $(foreach target,$(1),$(call gen_target_vars_and_deps,$(target),$(2)))
endef

#
# Rules Section
#

# the default target "all" ensures building of the targets. When multiple
# architectures are used, the targets are build in subdirs.

# the relink-rule: make the TARGETs phony. Specification of build-targets 
# in MAKECMDGOALS is not allowed. Possibility: TARGET=
#
ifneq ($(filter relink,$(MAKECMDGOALS)),)
.PHONY: $(TARGET)
relink: all
endif



###
#
# Compilation rules with dependency-generation
#
# If we have ld.so, we use it to create our dependencies (see MAKEDEP
# definition). Otherwise, we fall back to whatever we need. For
# compilation with gcc, this is using "-MD" and post-processing the
# generated files.

ifeq ($(CONFIG_HAVE_LDSO),)
LIBDEPS = $(foreach file, \
                    $(patsubst -l%,lib%.a,$(filter-out -L%,$(LDFLAGS))) \
                    $(patsubst -l%,lib%.so,$(filter-out -L%,$(LDFLAGS))),\
                    $(word 1, $(foreach dir, \
                           $(patsubst -L%,%,\
                           $(filter -L%,$(LDFLAGS) $(L4ALL_LIBDIR))),\
                      $(wildcard $(dir)/$(file)))))

DEPEND_EXTEND_CMD = $(AWKP) '			\
/^[^:]*: ..*/{					\
	while(sub("\\\\$$","")){		\
		getline nextline ;		\
		$$0=$$0 " " nextline		\
	}					\
	split($$0,field,": ");			\
	sub("^$(*F).o","$*.s $*.i $@",field[1]);	\
	nr=split(field[2],deps) ;		\
	for(i=1;i<=nr;){			\
	  printf("ifeq ($$(shell test y \\\n");	\
	  for(j=0; j<100 && i+j<=nr; j++){	\
	    printf("-a -r %s \\\n", deps[i+j]);	\
	  }					\
	  printf("&& echo y),)");		\
	  printf("\n%s: FORCE\nelse\n%s: $(BID_OBJ_Makefile) \\\n",	\
	  	field[1],field[1]);		\
	  for(j=0; j<100 && i+j<=nr; j++){	\
	    printf("%s \\\n",deps[i+j]);	\
	  }					\
	  printf("\nendif\n");			\
	  i+=j;					\
	}					\
}'
DEPEND_EXTEND_FUNC = ( $(DEPEND_EXTEND_CMD) < $(1) > $(2).new && rm -f $(1) && mv $(2).new $(2) ) || ( rm -f $(1) $(2).new $(2) ; $(DEPEND_IGNORE_ERRORS) )
DEPEND_FLAG = -MD

.PHONY: FORCE
endif # CONFIG_HAVE_LDSO empty
DEPFLAGS   = -MD -MP -MF $(@D)/.$(@F).d
DEPFLAGS_F = -cpp

include $(L4DIR)/mk/rules.inc

# generate rules to compile %.cc files to %.o, %.s.o etc
$(eval $(call BID_GENERATE_CXX_MAKE_RULES,cc))
# generate rules to compile %.cpp files to %.o, %.s.o etc
$(if $(BID_ALLOW_CPP_AS_CPLUSPLUS_FILE),$(eval $(call BID_GENERATE_CXX_MAKE_RULES,cpp)))
# generate rules to compile %.c files to %.o, %.s.o etc
$(eval $(call BID_GENERATE_C_MAKE_RULES,c))
# generate rules to compile %.f90 files to %.o, %.s.o etc
$(foreach e,$(FORTRAN_FILE_EXTENSIONS),$(eval $(call BID_GENERATE_F_MAKE_RULES,$(subst .,,$e))))
# generate rules to compile .rs (Rust) files to .o
$(foreach e,$(RUST_FILE_EXTENSIONS),$(eval $(call BID_GENERATE_RS_MAKE_RULES,$(subst .,,$e))))
# generate rules to compile %.S files to %.o, %.s.o etc
$(foreach e,$(BID_ASM_FILE_EXTENSIONS),$(eval $(call BID_GENERATE_ASM_MAKE_RULES,$(subst .,,$e))))

# Reset implicit rules, mind the TAB.
define reset_implicit_rule =
%o: %$(1)
	
endef

$(foreach e,.c .cc .cpp .rs $(BID_ASM_FILE_EXTENSIONS) $(FORTRAN_FILE_EXTENSIONS),$(eval $(call reset_implicit_rule,$e)))

$(eval $(call BID_GENERATE_I_MAKE_RULE,%.i,%.c,$(CC),$(CFLAGS) $(call BID_mode_var,NOPICFLAGS)))
$(eval $(call BID_GENERATE_I_MAKE_RULE,%.s.i,%.c,$(CC),$(CFLAGS) $(PICFLAGS)))
$(eval $(call BID_GENERATE_I_MAKE_RULE,%.i,%.cc,$(CXX),$(CXXFLAGS) $(call BID_mode_var,NOPICFLAGS)))
$(eval $(call BID_GENERATE_I_MAKE_RULE,%.s.i,%.cc,$(CXX),$(CXXFLAGS) $(PICFLAGS)))
$(if $(BID_ALLOW_CPP_AS_CPLUSPLUS_FILE),$(eval $(call BID_GENERATE_I_MAKE_RULE,%.i,%.cpp,$(CXX),$(CXXFLAGS) $(call BID_mode_var,NOPICFLAGS))))
$(if $(BID_ALLOW_CPP_AS_CPLUSPLUS_FILE),$(eval $(call BID_GENERATE_I_MAKE_RULE,%.s.i,%.cpp,$(CXX),$(CXXFLAGS) $(PICFLAGS))))
$(eval $(call BID_GENERATE_I_MAKE_RULE,%.i,%.S,$(CC),$(ASFLAGS)))

$(eval $(call BID_GENERATE_S_MAKE_RULE,%.c,$(CC),$(CFLAGS)))
$(eval $(call BID_GENERATE_S_MAKE_RULE,%.cc,$(CC),$(CXXFLAGS)))
$(if $(BID_ALLOW_CPP_AS_CPLUSPLUS_FILE),$(eval $(call BID_GENERATE_S_MAKE_RULE,%.cpp,$(CC),$(CXXFLAGS))))

%.c:	%.y
	@$(GEN_MESSAGE)
	$(VERBOSE)$(CONFIG_YACC) $(YFLAGS) $(call absfilename,$<)
	$(VERBOSE)mv -f y.tab.c $@
	$(VERBOSE)if [ -f y.tab.h ]; then mv -f y.tab.h $(@:.c=.h); fi

%.c:	%.l
	@$(COMP_MESSAGE)
	$(VERBOSE)$(CONFIG_LEX) -o$@ $(call absfilename,$<)

%.dpi:	%.dpe
	@$(GEN_MESSAGE)
	$(VERBOSE)$(call MAKEDEP,perl) $(GEN_DOPECODE) $< >$@

%.dtb: %.dts $(GENERAL_D_LOC)
	@$(BUILD_MESSAGE)
	$(VERBOSE)$(MKDIR) $(@D)
	$(VERBOSE)$(CPP) $(CFLAGS) $(addprefix -I, $(PRIVATE_INCDIR)) -x assembler-with-cpp -Wp,-MD,$(@D)/.$(@F).d,-MT$@,-MP -nostdinc -o $@.pre $<
	$(VERBOSE)$(DTC) $(DTC_FLAGS) $(addprefix -i , $(PRIVATE_INCDIR)) -i $(dir $<) -d $(call BID_dot_fname,$@).d-x -O dtb -b 0 -o $@ $@.pre
	$(VERBOSE)$(SED) -e "s|$@.pre|$<|" $(call BID_dot_fname,$@).d-x | \
		perl -ne 'print "$$_\n"; if (/^.*:\s*(.*)$$/) { print "$$_:\n" foreach (split(/ /,$$1)); }' >> $(@D)/.$(@F).d
	$(VERBOSE)$(RM) $(call BID_dot_fname,$@).d-x $@.pre

define bin_to_asm_recipe
	echo -e ".section .rodata, \"a\"\n"    \
	        ".global _binary_$(3)_start\n" \
	        ".global _binary_$(3)_end\n"   \
	        ".p2align 3\n"                 \
	        "_binary_$(3)_start:\n"        \
	        ".incbin \"$(1)\"\n"           \
	        "_binary_$(3)_end:\n" >$(2)
endef

SRC_DATA_VPATH ?= $(SRC_DIR)
$(foreach s,$(SRC_DATA),$(eval vpath $(s) $(SRC_DATA_VPATH)))

$(addsuffix .bin.S,$(SRC_DATA)): %.bin.S: % $(GENERAL_D_LOC)
	@$(GEN_MESSAGE)
	$(VERBOSE)$(MKDIR) $(@D)
	$(VERBOSE)$(call bin_to_asm_recipe,$<,$@,$(subst -,_,$(subst /,_,$(subst .,_,$(strip $(*F))))))

DEPS	+= $(foreach file,$(ALLOBJS), $(call BID_dot_fname,$(file)).d)
DEPS	+= $(foreach file,$(ALLDPI), $(call BID_dot_fname,$(file)).d)

# Common clean Rules

clean cleanall::
	$(VERBOSE)$(RM) $(strip $(filter-out $(KEEP_ON_CLEAN),\
		$(wildcard *.dpi) $(wildcard *.o) $(wildcard *.i) \
		$(wildcard *.S) \
		$(wildcard $(filter-out -%, $(ALLOBJS) $(DEL_ON_CLEAN)))))

# clean: delete all temporary editor files, objects, binaries
#        and most generated files
cleanall::
	$(VERBOSE)$(RM) $(TARGET) $(addsuffix .*.d,$(sort $(dir $(ALLOBJS)))) \
		$(wildcard $(BID_OBJ_Makefile).inc)

.PHONY: scrub clean cleanall disasm

endif	# $(SYSTEM) is defined

# General rules

# scrub: delete temporary editor files and stuff like this
#        can be extended by locals
scrub cleanall::
	$(VERBOSE)$(SCRUB)


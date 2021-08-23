#
# GLOBAL Makefile for the whole L4 tree
#

L4DIR		?= .

PRJ_SUBDIRS   := pkg tests $(wildcard l4linux)
BUILD_DIRS    := tool
install-dirs  := tool pkg
clean-dirs    := tool pkg tests doc
cleanall-dirs := tool pkg tests doc

BUILD_TOOLS    = bash bison flex gawk perl tput \
                 $(foreach v,CC CXX LD HOST_CC HOST_CXX HOST_LD, \
                           $(firstword $(foreach x,$($(v)),\
			                         $(if $(findstring =,$(x)),,$(x)))))
BUILD_TOOLS_pkg/uvmm  := dtc

CMDS_WITHOUT_OBJDIR := help checkbuild checkbuild.% up update check_build_tools
CMDS_PROJECT_MK     := all clean cleanall install scrub cont doc help \
                       $(wildcard $(MAKECMDGOALS))

# our default target is all::
all::

#####################
# config-tool

DROPSCONF 		= y
DROPSCONF_DEFCONFIG	?= $(L4DIR)/mk/defconfig/config.x86
KCONFIG_FILE            = $(OBJ_BASE)/Kconfig.generated
KCONFIG_FILE_DEPS       = $(OBJ_BASE)/.Kconfig.generated.d
KCONFIG_FILE_SRC        = $(L4DIR)/mk/Kconfig
DROPSCONF_CONFIG	= $(OBJ_BASE)/.kconfig.auto
DROPSCONF_CONFIG_H	= $(OBJ_BASE)/include/l4/bid_config.h
DROPSCONF_CONFIG_MK	= $(OBJ_BASE)/.config.all
DROPSCONF_DONTINC_MK	= y
DROPSCONF_HELPFILE	= $(L4DIR)/mk/config.help

# separation in "dependent" (ie opts the build output depends on) and
# "independent" (ie opts the build output does not depend on) opts
CONFIG_MK_INDEPOPTS	= CONFIG_BID_GENERATE_MAPFILE \
			  CONFIG_DEPEND_VERBOSE \
			  CONFIG_VERBOSE_SWITCH \
			  CONFIG_BID_COLORED_PHASES CONFIG_HAVE_LDSO \
			  CONFIG_INT_CPP_NAME_SWITCH BID_LIBGENDEP_PATHS CONFIG_INT_CPP_.*_NAME \
			  CONFIG_INT_CXX_.*_NAME CONFIG_VERBOSE CONFIG_BID_STRIP_PROGS \
			  CONFIG_INT_LD_NAME_SWITCH CONFIG_INT_LD_.*_NAME
CONFIG_MK_PLATFORM_OPTS = CONFIG_PLATFORM_.*
CONFIG_MK_REAL		= $(OBJ_BASE)/.config
CONFIG_MK_INDEP		= $(OBJ_BASE)/.config.indep
CONFIG_MK_PLATFORM      = $(OBJ_BASE)/.config.platform

INCLUDE_BOOT_CONFIG    := optional

ifneq ($(filter $(CMDS_WITHOUT_OBJDIR),$(MAKECMDGOALS)),)
IGNORE_MAKECONF_INCLUDE=1
endif

ifneq ($(B)$(BUILDDIR_TO_CREATE),)
IGNORE_MAKECONF_INCLUDE=1
endif

ifeq ($(IGNORE_MAKECONF_INCLUDE),)
ifneq ($(filter help config oldconfig olddefconfig silentoldconfig,$(MAKECMDGOALS)),)
# tweek $(L4DIR)/mk/Makeconf to use the intermediate file
export BID_IGN_ROOT_CONF=y
BID_ROOT_CONF=$(DROPSCONF_CONFIG_MK)
endif

# $(L4DIR)/mk/Makeconf shouln't include Makeconf.local twice
MAKECONFLOCAL		= /dev/null


# Use project.mk if we use the default goal (MAKECMDGOALS is empty)
# or any of the goals listed in CMDS_PROJECT_MK, this saves us
# from running the time consuming project.mk find operations.
ifeq ($(MAKECMDGOALS),)
  include $(L4DIR)/mk/project.mk
else
  ifneq ($(filter $(CMDS_PROJECT_MK),$(MAKECMDGOALS)),)
    include $(L4DIR)/mk/project.mk
  else
    include $(L4DIR)/mk/Makeconf
  endif
endif

PKGDEPS_IGNORE_MISSING :=
export DROPS_STDDIR

# after having absfilename, we can export BID_ROOT_CONF
ifneq ($(filter config oldconfig olddefconfig silentoldconfig gconfig nconfig xconfig, $(MAKECMDGOALS)),)
export BID_ROOT_CONF=$(call absfilename,$(OBJ_BASE))/.config.all
endif
endif

#####################
# rules follow

ifneq ($(strip $(B)),)
BUILDDIR_TO_CREATE := $(B)
endif
ifneq ($(strip $(BUILDDIR_TO_CREATE)),)
ifneq ($(wildcard $(L4DIR)/mk/defconfig/config.$(T)),)
  DROPSCONF_DEFCONFIG=$(L4DIR)/mk/defconfig/config.$(T)
endif
all::
	@echo "Creating build directory \"$(BUILDDIR_TO_CREATE)\"..."
	@if [ -e "$(BUILDDIR_TO_CREATE)" ]; then	\
		echo "Already exists, aborting.";	\
		exit 1;					\
	fi
	@mkdir -p "$(BUILDDIR_TO_CREATE)"
	@cp $(DROPSCONF_DEFCONFIG) $(BUILDDIR_TO_CREATE)/.kconfig
	@echo CONFIG_PLATFORM_TYPE_$(PT)=y >> $(BUILDDIR_TO_CREATE)/.kconfig
	@$(MAKE) B= BUILDDIR_TO_CREATE= O=$(BUILDDIR_TO_CREATE) olddefconfig \
	  || ( $(RM) -r "$(BUILDDIR_TO_CREATE)" ; exit 1 )
	@echo "done."
else

all:: $(BUILD_DIRS) $(if $(S),,l4defs)

endif


#
# The following targets do work without explicit subdirs
# ('S=...') only.
#
ifeq ($(S),)

# some special cases for dependencies follow:
# L4Linux depends on the availability of the l4defs
l4linux: l4defs
l4linux/l4-build: l4defs

# hack for examples, they virtually depend on anything else
pkg/examples: $(filter-out pkg/examples,$(BUILD_SUBDIRS))

# some more dependencies
tool: $(DROPSCONF_CONFIG_MK)
$(BUILD_SUBDIRS):  $(DROPSCONF_CONFIG_MK) tool

ifneq ($(CONFIG_BID_BUILD_DOC),)
install-dirs += doc
all:: doc
endif

up update:
	$(VERBOSE)svn up -N
	$(VERBOSE)svn up mk tool/gendep tool/kconfig tool/elf-patcher doc/source conf tool/lib tool/vim tool/bin
	$(VERBOSE)$(MAKE) -C pkg up

tool:
	$(VERBOSE)if [ -r $@/Makefile ]; then PWD=$(PWD)/$@ $(MAKE) -C $@; fi

doc:
	$(VERBOSE)if [ -r doc/source/Makefile ]; then PWD=$(PWD)/doc/source $(MAKE) -C doc/source; fi

BID_POST_CONT_HOOK := $(MAKE) regen_l4defs

.PHONY: all clean cleanall clean-test-scripts install up update doc
.PHONY: $(BUILD_DIRS) doc check_build_tools cont cleanfast

cleanfast:
	$(VERBOSE)if [ -f $(OBJ_BASE)/include/l4/bid_config.h ]; then  \
	            cp -a $(OBJ_BASE)/include/l4/bid_config.h          \
	                  $(OBJ_BASE)/.tmp.bid_config.h;               \
	          fi
	$(VERBOSE)$(RM) -r $(addprefix $(OBJ_BASE)/,bin include pkg tests doc ext-pkg pc lib test l4defs.mk.inc l4defs.sh.inc) \
	                   $(IMAGES_DIR)
	$(VERBOSE)if [ -f $(OBJ_BASE)/.tmp.bid_config.h ]; then        \
	            mkdir -p $(OBJ_BASE)/include/l4;                   \
	            mv $(OBJ_BASE)/.tmp.bid_config.h                   \
	               $(OBJ_BASE)/include/l4/bid_config.h;            \
	          fi

cleanall::
	$(VERBOSE)rm -f *~

clean-test-scripts:
	$(VERBOSE)$(RM) -r $(OBJ_BASE)/test/t

clean cleanall install::
	$(VERBOSE)set -e; for i in $($@-dirs) ; do \
	  if [ -r $$i/Makefile -o -r $$i/GNUmakefile ] ; then \
		PWD=$(PWD)/$$i $(MAKE) -C $$i $@ ; fi ; done

L4DEF_FILE_MK ?= $(OBJ_BASE)/l4defs.mk.inc
L4DEF_FILE_SH ?= $(OBJ_BASE)/l4defs.sh.inc

l4defs: $(L4DEF_FILE_MK) $(L4DEF_FILE_SH)

generate_l4defs_files = \
	$(VERBOSE)tmpdir=$(OBJ_BASE)/l4defs.gen.dir &&                 \
	mkdir -p $$tmpdir &&                                           \
	echo "L4DIR = $(L4DIR_ABS)"                      > $$tmpdir/Makefile && \
	echo "OBJ_BASE = $(OBJ_BASE)"                   >> $$tmpdir/Makefile && \
	echo "L4_BUILDDIR = $(OBJ_BASE)"                >> $$tmpdir/Makefile && \
	echo "SRC_DIR = $$tmpdir"                       >> $$tmpdir/Makefile && \
	echo "PKGDIR_ABS = $(L4DIR_ABS)/l4defs.gen.dir" >> $$tmpdir/Makefile && \
	echo "BUILD_MESSAGE ="                          >> $$tmpdir/Makefile && \
	cat $(L4DIR)/mk/export_defs.inc                 >> $$tmpdir/Makefile && \
	PWD=$$tmpdir $(MAKE) -C $$tmpdir -f $$tmpdir/Makefile          \
	  CALLED_FOR=$(1) L4DEF_FILE_MK=$(L4DEF_FILE_MK) L4DEF_FILE_SH=$(L4DEF_FILE_SH) && \
	$(RM) -r $$tmpdir

$(L4DEF_FILE_MK): $(OBJ_DIR)/.Package.deps pkg/l4re-core \
                  $(DROPSCONF_CONFIG_MK) $(L4DIR)/mk/export_defs.inc
	+$(call generate_l4defs_files,static)
	+$(call generate_l4defs_files,minimal)
	+$(call generate_l4defs_files,shared)
	+$(call generate_l4defs_files,sharedlib)

$(L4DEF_FILE_SH): $(L4DEF_FILE_MK)

regen_l4defs:
	+$(call generate_l4defs_files,static)
	+$(call generate_l4defs_files,minimal)
	+$(call generate_l4defs_files,shared)
	+$(call generate_l4defs_files,sharedlib)

.PHONY: l4defs regen_l4defs
endif # empty $(S)

#####################
# config-rules follow

HOST_SYSTEM := $(shell uname | tr 'A-Z' 'a-z')
export HOST_SYSTEM

# it becomes a bit confusing now: 'make config' results in a config file, which
# must be postprocessed. This is done in config.inc. Then,
# we evaluate some variables that depend on the postprocessed config file.
# The variables are defined in mk/Makeconf, which sources Makeconf.bid.local.
# Hence, we have to 1) postprocess, 2) call make again to get the variables.
BID_DCOLON_TARGETS += DROPSCONF_CONFIG_MK_POST_HOOK
DROPSCONF_CONFIG_MK_POST_HOOK:: check_build_tools $(OBJ_DIR)/Makefile
        # libgendep must be done before calling make with the local helper
	$(VERBOSE)$(MAKE) libgendep
	$(VERBOSE)$(MAKE) Makeconf.bid.local-helper || \
		(rm -f $(DROPSCONF_CONFIG_MK) $(CONFIG_MK_REAL) $(CONFIG_MK_INDEP); false)
	$(VEROBSE)$(LN) -snf $(L4DIR_ABS) $(OBJ_BASE)/source
	$(VERBOSE)$(MAKE) checkconf

KCONFIGS_ARCH     := $(wildcard $(L4DIR)/mk/arch/Kconfig.*.inc)
KCONFIG_PLATFORMS := $(wildcard $(L4DIR)/mk/platforms/*.conf $(L4DIR)/conf/platforms/*.conf)

$(KCONFIG_FILE)%platform_types $(KCONFIG_FILE)%platforms $(KCONFIG_FILE)%platforms.list: Makefile $(L4DIR)/tool/bin/gen_kconfig_includes \
                                                          $(KCONFIG_PLATFORMS)
	$(file >$(KCONFIG_FILE_DEPS).platforms,$(KCONFIG_FILE): $^)
	$(foreach f,$^,$(file >>$(KCONFIG_FILE_DEPS).platforms,$(f):))
	$(VERBOSE)MAKE="$(MAKE)"; $(L4DIR)/tool/bin/gen_kconfig_includes $(KCONFIG_FILE) $(KCONFIG_PLATFORMS)

$(KCONFIG_FILE): $(KCONFIG_FILE_SRC) Makefile $(KCONFIGS_ARCH) $(L4DIR)/tool/bin/gen_kconfig\
                 | $(KCONFIG_FILE).platform_types $(KCONFIG_FILE).platforms $(KCONFIG_FILE).platforms.list
	$(file >$(KCONFIG_FILE_DEPS),$(KCONFIG_FILE): $^)
	$(foreach f,$^,$(file >>$(KCONFIG_FILE_DEPS),$(f):))
	$(VERBOSE)$(L4DIR)/tool/bin/gen_kconfig $(KCONFIG_FILE_SRC) $(KCONFIG_FILE) $(KCONFIGS_ARCH)

-include $(KCONFIG_FILE_DEPS) $(KCONFIG_FILE_DEPS).platforms

checkconf:
	$(VERBOSE)if [ -n "$(GCCDIR)" -a ! -e $(GCCDIR)/include/stddef.h ]; then \
	  $(ECHO); \
	  $(ECHO) "$(GCCDIR) seems wrong (stddef.h not found)."; \
	  $(ECHO) "Does it exist?"; \
	  $(ECHO); \
	  exit 1; \
	fi
	$(VERBOSE)if [ -z "$(filter $(CC_WHITELIST-$(BID_COMPILER_TYPE)), \
	                            $(GCCVERSION))" ]; then \
	  $(ECHO); \
	  $(ECHO) "$(BID_COMPILER_TYPE)-$(GCCVERSION) is not supported."; \
	  $(ECHO) "Please use a $(BID_COMPILER_TYPE) of the following" \
	          "versions: $(CC_WHITELIST-$(BID_COMPILER_TYPE))"; \
	  $(ECHO); \
	  exit 1; \
	fi
	$(VERBOSE)if [ -n "$(filter $(CC_BLACKLIST-$(BUILD_ARCH)-gcc), \
	                            $(GCCVERSION).$(GCCPATCHLEVEL))" ]; then \
	  $(ECHO); \
	  $(ECHO) "GCC-$(GCCVERSION).$(GCCPATCHLEVEL) is blacklisted" \
	          "because it showed to produce wrong results."; \
	  $(ECHO) "Please upgrade to a more recent version."; \
	  $(ECHO); \
	  exit 1; \
	fi


# caching of some variables. Others are determined directly.
# The contents of the variables to cache is already defined in mk/Makeconf.
.PHONY: Makeconf.bid.local-helper Makeconf.bid.local-internal-names \
        libgendep checkconf
ARCH = $(BUILD_ARCH)
CC := $(if $(filter sparc,$(ARCH)),$(if $(call GCCIS_sparc_leon_f),sparc-elf-gcc,$(CC)),$(CC))
LD := $(if $(filter sparc,$(ARCH)),$(if $(call GCCIS_sparc_leon_f),sparc-elf-ld,$(LD)),$(LD))
Makeconf.bid.local-helper:
	$(VERBOSE)echo BUILD_SYSTEMS="$(strip $(ARCH)_$(CPU)            \
	               $(ARCH)_$(CPU)-$(BUILD_ABI))" >> $(DROPSCONF_CONFIG_MK)
	$(VERBOSE)$(foreach v, BID_COMPILER_TYPE GCCDIR GCCLIB_HOST GCCLIB_EH GCCLIB_S_SO \
	                GCCVERSION GCCMAJORVERSION GCCMINORVERSION      \
			GCCPATCHLEVEL GCC_HAS_ATOMICS GCCPREFIXOPT      \
			GCCNOSTACKPROTOPT GCCSTACKPROTOPT GCCSTACKPROTALLOPT LDVERSION \
			GCCWNONOEXCEPTTYPE GCCWNOPSABI GCCSYSLIBDIRS GCCFORTRANAVAIL \
			GCCINCFIXEDPATH CONDITIONAL_WARNINGS_MEDIUM CONDITIONAL_WARNINGS_FULL \
			$(if $(GCCNOFPU_$(ARCH)_f),GCCNOFPU_$(ARCH))    \
			$(if $(GCCIS_$(ARCH)_leon_f),GCCIS_$(ARCH)_leon),   \
			echo $(v)=$(call $(v)_f,$(ARCH))                \
			>>$(DROPSCONF_CONFIG_MK);)
	$(VERBOSE)$(foreach v, crtbegin.o crtbeginS.o crtbeginT.o \
	                       crtendS.o crtend.o, \
			echo GCCLIB_FILE_$(v)=$(call GCCLIB_file_f,$(v))   \
			>>$(DROPSCONF_CONFIG_MK);)
	$(VERBOSE)$(foreach v, LD_GENDEP_PREFIX, echo $v=$($(v)) >>$(DROPSCONF_CONFIG_MK);)
	$(VERBOSE)echo "HOST_SYSTEM=$(HOST_SYSTEM)" >>$(DROPSCONF_CONFIG_MK)
	$(VERBOSE)echo "LD_HAS_HASH_STYLE_OPTION=$(shell if $(LD) --help 2>&1 | grep -q ' --hash-style='; then echo y; else echo n; fi)" >>$(DROPSCONF_CONFIG_MK)
	$(VERBOSE)# we need to call make again, because HOST_SYSTEM (set above) must be
	$(VERBOSE)# evaluated for LD_PRELOAD to be set, which we need in the following
	$(VERBOSE)$(MAKE) Makeconf.bid.local-internal-names
	$(VERBOSE)sort <$(DROPSCONF_CONFIG_MK) >$(CONFIG_MK_REAL).tmp
	$(VERBOSE)echo -e "# Automatically generated. Don't edit\n" >$(CONFIG_MK_INDEP)
	$(VERBOSE)echo -e "# Automatically generated. Don't edit\n" >$(CONFIG_MK_PLATFORM)
	$(VERBOSE)grep $(addprefix -e ^,$(CONFIG_MK_INDEPOPTS) )    <$(CONFIG_MK_REAL).tmp >>$(CONFIG_MK_INDEP)
	$(VERBOSE)grep $(addprefix -e ^,$(CONFIG_MK_PLATFORM_OPTS)) <$(CONFIG_MK_REAL).tmp >>$(CONFIG_MK_PLATFORM)
	$(VERBOSE)echo -e "# Automatically generated. Don't edit\n" >$(CONFIG_MK_REAL).tmp2
	$(VERBOSE)grep -v $(addprefix -e ^,$$ # $(CONFIG_MK_INDEPOPTS) $(CONFIG_MK_PLATFORM_OPTS)) \
		<$(CONFIG_MK_REAL).tmp >>$(CONFIG_MK_REAL).tmp2
	$(VERBOSE)echo -e 'include $(call absfilename,$(CONFIG_MK_INDEP))' >>$(CONFIG_MK_REAL).tmp2
	$(VERBOSE)echo -e 'include $(call absfilename,$(CONFIG_MK_PLATFORM))' >>$(CONFIG_MK_REAL).tmp2
	$(VERBOSE)if [ -e "$(CONFIG_MK_REAL)" ]; then                        \
	            diff --brief -I ^COLOR_TERMINAL $(CONFIG_MK_REAL) $(CONFIG_MK_REAL).tmp2 || \
		      mv $(CONFIG_MK_REAL).tmp2 $(CONFIG_MK_REAL);           \
		  else                                                       \
		    mv $(CONFIG_MK_REAL).tmp2 $(CONFIG_MK_REAL);             \
		  fi
	$(VERBOSE)$(RM) $(CONFIG_MK_REAL).tmp $(CONFIG_MK_REAL).tmp2

Makeconf.bid.local-internal-names:
ifneq ($(CONFIG_INT_CPP_NAME_SWITCH),)
	$(VERBOSE) set -e; X="$(OBJ_BASE)/tmp.$$$$$$RANDOM.c" ; \
		echo 'int main(void){}'>$$X ; \
		rm -f $$X.out ; $(LD_GENDEP_PREFIX) GENDEP_SOURCE=$$X \
		GENDEP_OUTPUT=$$X.out $(CC) $(CCXX_FLAGS) -c $$X -o $$X.o; \
		if [ ! -e $$X.out ]; then \
			echo -e "\n\033[1;31mGendep did not generate output. Is the compiler ($(CC)) statically linked?\033[0m"; \
			echo -e "Please use a dynamically linked compiler.\n"; exit 1; \
		fi; echo INT_CPP_NAME=`cat $$X.out` \
			>>$(DROPSCONF_CONFIG_MK); \
		rm -f $$X $$X.{o,out};
	$(VERBOSE)set -e; X="$(OBJ_BASE)/tmp.$$$$$$RANDOM.cc" ; \
		echo 'int main(void){}'>$$X; \
		rm -f $$X.out; $(LD_GENDEP_PREFIX) GENDEP_SOURCE=$$X \
		GENDEP_OUTPUT=$$X.out $(CXX) -c $$X -o $$X.o; \
		test -e $$X.out; echo INT_CXX_NAME=`cat $$X.out` \
			>>$(DROPSCONF_CONFIG_MK); \
		rm -f $$X $$X.{o,out};
endif
ifneq ($(CONFIG_INT_LD_NAME_SWITCH),)
	$(VERBOSE) set -e; echo INT_LD_NAME=$$($(LD) 2>&1 | perl -p -e 's,^(.+/)?(.+):.+,$$2,') >> $(DROPSCONF_CONFIG_MK)
endif
	$(VERBOSE)emulations=$$(LC_ALL=C $(firstword $(LD)) --help |     \
	                        grep -i "supported emulations:" |        \
	                        sed -e 's/.*supported emulations: //') ; \
	unset found_it;                                                  \
	for e in $$emulations; do                                        \
	  for c in $(LD_EMULATION_CHOICE_$(ARCH)); do                    \
	    if [ "$$e" = "$$c" ]; then                                   \
	      echo LD_EMULATION=$$e >> $(DROPSCONF_CONFIG_MK);           \
	      found_it=1;                                                \
	      break;                                                     \
	    fi;                                                          \
	  done;                                                          \
	  if [ "$$found_it" = "1" ]; then                                \
	    break;                                                       \
	  fi;                                                            \
	done;                                                            \
	if [ "$$found_it" != "1" ]; then                                 \
	  echo "No known ld emulation found"; exit 1;                    \
	fi

libgendep:
	$(VERBOSE)if [ ! -r tool/gendep/Makefile ]; then	\
	            echo "=== l4/tool/gendep missing! ===";	\
		    exit 1;					\
	          fi
	$(VERBOSE)PWD=$(PWD)/tool/gendep $(MAKE) -C tool/gendep

DIRS_FOR_BUILD_TOOLS_CHECKS = $(patsubst BUILD_TOOLS_%,%,    \
                                         $(filter BUILD_TOOLS_%,$(.VARIABLES)))
BUILD_TOOLS += $(foreach dir,$(DIRS_FOR_BUILD_TOOLS_CHECKS), \
                         $(if $(wildcard $(L4DIR)/$(dir)),   \
                              $(BUILD_TOOLS_$(dir))))

check_build_tools:
	@unset mis;                                                \
	for i in $(sort $(BUILD_TOOLS)); do                        \
	  if ! command -v $$i >/dev/null 2>&1; then                \
	    [ -n "$$mis" ] && mis="$$mis ";                        \
	    mis="$$mis$$i";                                        \
	  fi                                                       \
	done;                                                      \
	if [ -n "$$mis" ]; then                                    \
	  echo -e "\033[1;31mProgram(s) \"$$mis\" not found, please install!\033[0m"; \
	  exit 1;                                                  \
	else                                                       \
	  echo "All build tools checked ok.";                      \
	fi

define common_envvars
	ARCH="$(ARCH)" PLATFORM_TYPE="$(PLATFORM_TYPE)"
endef
define tool_envvars
	L4DIR=$(L4DIR)                                           \
	SEARCHPATH="$(MODULE_SEARCH_PATH):$(BUILDDIR_SEARCHPATH)"
endef
define set_ml
	unset ml; ml=$(L4DIR_ABS)/conf/modules.list;             \
	   [ -n "$(MODULES_LIST)" ] && ml=$(MODULES_LIST)
endef
define entryselection
	   unset e;                                              \
	   $(set_ml);                                            \
	   [ -n "$(ENTRY)"       ] && e="$(ENTRY)";              \
	   [ -n "$(E)"           ] && e="$(E)";                  \
	   if [ -z "$$e" ]; then                                 \
	     BACKTITLE="No entry given. Use                      \
	                'make $@ E=entryname' to avoid menu."    \
	       L4DIR=$(L4DIR) $(common_envvars)                  \
	       $(L4DIR)/tool/bin/entry-selector menu $$ml        \
	         2> $(OBJ_BASE)/.entry-selector.tmp;             \
	     if [ $$? != 0 ]; then                               \
	       cat $(OBJ_BASE)/.entry-selector.tmp;              \
	       exit 1;                                           \
	     fi;                                                 \
	     e=$$(cat $(OBJ_BASE)/.entry-selector.tmp);          \
	     $(RM) $(OBJ_BASE)/.entry-selector.tmp;              \
	   fi
endef
define checkx86amd64build
	$(VERBOSE)if [ "$(ARCH)" != "x86" -a "$(ARCH)" != "amd64" ]; then      \
	  echo "This mode can only be used with architectures x86 and amd64."; \
	  exit 1;                                                              \
	fi
endef
define genimage
	+$(VERBOSE)$(entryselection);                                                 \
	$(MKDIR) $(IMAGES_DIR);                                                       \
	PWD=$(PWD)/pkg/bootstrap/server/src $(common_envvars)                         \
	    QEMU_BINARY_NAME=$(if $(QEMU_PATH),$(QEMU_PATH),$(QEMU_ARCH_MAP_$(ARCH))) \
	    $(MAKE) -C pkg/bootstrap/server/src ENTRY="$$e"                           \
	            BOOTSTRAP_MODULES_LIST=$$ml $(1)                                  \
		    BOOTSTRAP_MODULE_PATH_BINLIB="$(BUILDDIR_SEARCHPATH)"             \
		    BOOTSTRAP_SEARCH_PATH="$(MODULE_SEARCH_PATH)"
endef

define switch_ram_base_func
	echo "  ... Regenerating RAM_BASE settings"; set -e; \
	echo "# File semi-automatically generated by 'make switch_ram_base'" > $(OBJ_BASE)/Makeconf.ram_base; \
	echo "RAM_BASE := $(1)"                                             >> $(OBJ_BASE)/Makeconf.ram_base; \
	echo "RAM_BASE_SWITCH_PLATFORM_TYPE := $(PLATFORM_TYPE)"            >> $(OBJ_BASE)/Makeconf.ram_base; \
	PWD=$(PWD)/pkg/l4re-core/sigma0/server/src $(MAKE) RAM_BASE=$(1) -C pkg/l4re-core/sigma0/server/src;                      \
	PWD=$(PWD)/pkg/l4re-core/moe/server/src    $(MAKE) RAM_BASE=$(1) -C pkg/l4re-core/moe/server/src;                         \
	echo "RAM_BASE_SWITCH_OK := yes"                                    >> $(OBJ_BASE)/Makeconf.ram_base
endef

BUILDDIR_SEARCHPATH = $(OBJ_BASE)/bin/$(ARCH)_$(CPU):$(OBJ_BASE)/bin/$(ARCH)_$(CPU)/$(BUILD_ABI):$(OBJ_BASE)/lib/$(ARCH)_$(CPU):$(OBJ_BASE)/lib/$(ARCH)_$(CPU)/$(BUILD_ABI):$(OBJ_BASE)/assets

QEMU_ARCH_MAP_$(ARCH) ?= qemu-system-$(ARCH)

QEMU_KERNEL_TYPE          ?= elfimage
QEMU_KERNEL_FILE-elfimage  = $(IMAGES_DIR)/bootstrap.elf
QEMU_KERNEL_FILE-uimage    = $(IMAGES_DIR)/bootstrap.uimage
QEMU_KERNEL_FILE-itb       = $(IMAGES_DIR)/bootstrap.itb
QEMU_KERNEL_FILE-rawimage  = $(IMAGES_DIR)/bootstrap.raw
QEMU_KERNEL_FILE          ?= $(QEMU_KERNEL_FILE-$(QEMU_KERNEL_TYPE))

FASTBOOT_BOOT_CMD    ?= fastboot boot

check_and_adjust_ram_base:
	$(VERBOSE)if [ -z "$(PLATFORM_RAM_BASE)" ]; then          \
	  echo "ERROR: Platform \"$(PLATFORM_TYPE)\" not known."; \
	  echo "Available platforms:";                            \
	  $(MAKE) listplatforms;                                  \
	  exit 1;                                                 \
	fi
	$(VERBOSE)if [ -z "$(filter $(ARCH),$(PLATFORM_ARCH))" ]; then     \
	  echo "Platform \"$(PLATFORM_TYPE)\" not available for $(ARCH)."; \
	  exit 1;                                                          \
	fi
	+$(VERBOSE)if [ -n "$(RAM_BASE_SWITCH_OK)" ] && [ "$(RAM_BASE_SWITCH_PLATFORM_TYPE)" != "$(PLATFORM_TYPE)" ] && [ $$(($(RAM_BASE))) != $$(($(PLATFORM_RAM_BASE))) ] \
	                || [ -z "$(RAM_BASE)" ] || [ -z "$(RAM_BASE_SWITCH_OK)" ]; then \
	  echo "=========== Updating RAM_BASE for platform $(PLATFORM_TYPE) to $(PLATFORM_RAM_BASE) =========" ; \
	  $(call switch_ram_base_func,$(PLATFORM_RAM_BASE)); \
	fi

listentries:
	$(VERBOSE)$(set_ml); $(common_envvars) \
	  L4DIR=$(L4DIR) $(L4DIR)/tool/bin/entry-selector list $$ml

shellcodeentry:
	$(VERBOSE)$(entryselection);                                      \
	 SHELLCODE="$(SHELLCODE)" $(common_envvars) $(tool_envvars)       \
	  $(L4DIR)/tool/bin/shell-entry $$ml "$$e"

elfimage: check_and_adjust_ram_base
	$(call genimage,BOOTSTRAP_DO_UIMAGE= BOOTSTRAP_DO_RAW_IMAGE=)
	$(VERBOSE)$(if $(POST_IMAGE_CMD),$(call POST_IMAGE_CMD,$(IMAGES_DIR)/bootstrap.elf))

uimage: check_and_adjust_ram_base
	$(call genimage,BOOTSTRAP_DO_UIMAGE=y BOOTSTRAP_DO_RAW_IMAGE=)
	$(VERBOSE)$(if $(POST_IMAGE_CMD),$(call POST_IMAGE_CMD,$(IMAGES_DIR)/bootstrap.uimage))

itb: check_and_adjust_ram_base
	$(call genimage,BOOTSTRAP_DO_ITB=y)
	$(VERBOSE)$(if $(POST_IMAGE_CMD),$(call POST_IMAGE_CMD,$(IMAGES_DIR)/bootstrap.itb))

rawimage: check_and_adjust_ram_base
	$(call genimage,BOOTSTRAP_DO_UIMAGE= BOOTSTRAP_DO_RAW_IMAGE=y)
	$(VERBOSE)$(if $(POST_IMAGE_CMD),$(call POST_IMAGE_CMD,$(IMAGES_DIR)/bootstrap.raw))

fastboot fastboot_rawimage: rawimage
	$(VERBOSE)$(FASTBOOT_BOOT_CMD) \
	  $(if $(FASTBOOT_IMAGE),$(FASTBOOT_IMAGE),$(IMAGES_DIR)/bootstrap.raw)

fastboot_uimage: uimage
	$(VERBOSE)$(FASTBOOT_BOOT_CMD) \
	  $(if $(FASTBOOT_IMAGE),$(FASTBOOT_IMAGE),$(IMAGES_DIR)/bootstrap.uimage)

efiimage: check_and_adjust_ram_base
	$(checkx86amd64build)
	$(call genimage,BOOTSTRAP_DO_UIMAGE= BOOTSTRAP_DO_RAW_IMAGE= BOOTSTRAP_DO_UEFI=y)

ifneq ($(filter $(ARCH),x86 amd64),)
qemu:
	$(VERBOSE)$(entryselection);                                      \
	 qemu=$(if $(QEMU_PATH),$(QEMU_PATH),$(QEMU_ARCH_MAP_$(ARCH)));   \
	 QEMU=$$qemu QEMU_OPTIONS="$(QEMU_OPTIONS)"                       \
	  $(tool_envvars) $(common_envvars)                               \
	  $(L4DIR)/tool/bin/qemu-x86-launch $$ml "$$e"
else
qemu: $(QEMU_KERNEL_TYPE)
	$(VERBOSE)qemu=$(if $(QEMU_PATH),$(QEMU_PATH),$(QEMU_ARCH_MAP_$(ARCH))); \
	if [ -z "$$qemu" ]; then echo "Set QEMU_PATH!"; exit 1; fi;              \
	echo QEMU-cmd: $$qemu -kernel $(QEMU_KERNEL_FILE) $(QEMU_OPTIONS);    \
	$$qemu -kernel $(QEMU_KERNEL_FILE) $(QEMU_OPTIONS)
endif

vbox: $(if $(VBOX_ISOTARGET),$(VBOX_ISOTARGET),grub2iso)
	$(checkx86amd64build)
	$(VERBOSE)if [ -z "$(VBOX_VM)" ]; then                                 \
	  echo "Need to set name of configured VirtualBox VM im 'VBOX_VM'.";   \
	  exit 1;                                                              \
	fi
	$(VERBOSE)VirtualBox                    \
	    --startvm $(VBOX_VM)                \
	    --cdrom $(IMAGES_DIR)/.current.iso  \
	    --boot d                            \
	    $(VBOX_OPTIONS)

kexec:
	$(VERBOSE)$(entryselection);                        \
	 $(tool_envvars) $(common_envvars)                  \
	  $(L4DIR)/tool/bin/kexec-launch $$ml "$$e"

ux:
	$(VERBOSE)if [ "$(ARCH)" != "x86" ]; then                   \
	  echo "This mode can only be used with architecture x86."; \
	  exit 1;                                                   \
	fi
	$(VERBOSE)$(entryselection);                                 \
	$(tool_envvars)  $(common_envvars)                           \
	  $(if $(UX_GFX),UX_GFX="$(UX_GFX)")                         \
	  $(if $(UX_GFX_CMD),UX_GFX_CMD="$(UX_GFX_CMD)")             \
	  $(if $(UX_NET),UX_NET="$(UX_NET)")                         \
	  $(if $(UX_NET_CMD),UX_NET_CMD="$(UX_NET_CMD)")             \
	  $(if $(UX_GDB_CMD),UX_GDB_CMD="$(UX_GDB_CMD)")             \
	  $(L4DIR)/tool/bin/ux-launch $$ml "$$e" $(UX_OPTIONS)

GRUB_TIMEOUT ?= 0

define geniso
	$(checkx86amd64build)
	$(VERBOSE)$(entryselection);                                         \
	 $(MKDIR) $(IMAGES_DIR);                                             \
	 ISONAME=$(IMAGES_DIR)/$$(echo $$e | tr '[ A-Z]' '[_a-z]').iso;      \
	 $(tool_envvars) $(common_envvars)                                   \
	  $(L4DIR)/tool/bin/gengrub$(1)iso --timeout=$(GRUB_TIMEOUT) $$ml    \
	     $$ISONAME "$$e"                                                 \
	  && $(LN) -f $$ISONAME $(IMAGES_DIR)/.current.iso
endef

grub1iso:
	$(call geniso,1)

grub2iso:
	$(call geniso,2)

exportpack: $(if $(filter $(ARCH),x86 amd64),,$(QEMU_KERNEL_TYPE))
	$(if $(EXPORTPACKTARGETDIR),, \
	  @echo Need to specific target directory as EXPORTPACKTARGETDIR=dir; exit 1)
	$(VERBOSE)$(entryselection);                                      \
	 TARGETDIR=$(EXPORTPACKTARGETDIR);                                \
	 qemu=$(if $(QEMU_PATH),$(QEMU_PATH),$(QEMU_ARCH_MAP_$(ARCH)));   \
	 QEMU=$$qemu L4DIR=$(L4DIR) QEMU_OPTIONS="$(QEMU_OPTIONS)"        \
	 OUTPUT_DIR="$(BOOTSTRAP_OUTPUT_DIR)"                             \
	 IMAGE_FILE="$(QEMU_KERNEL_FILE)"                                 \
	 $(tool_envvars) $(common_envvars)                                \
	  $(L4DIR)/tool/bin/genexportpack --timeout=$(GRUB_TIMEOUT)       \
	                                  --grubpathprefix="$(GRUB_PATHPREFIX)" \
	                                  --grubentrytitle="$(GRUB_ENTRY_TITLE)" \
	                                   $$ml $$TARGETDIR $$e;

help::
	@echo
	@echo "Image generation targets:"
	@echo "  efiimage   - Generate an EFI image, containing all modules."
	@echo "  elfimage   - Generate an ELF image, containing all modules."
	@echo "  rawimage   - Generate a raw image (memory dump), containing all modules."
	@echo "  uimage     - Generate a uimage for u-boot, containing all modules."
	@echo "  itb        - Generate a FIT image for u-boot, containing all modules."
	@echo "  grub1iso   - Generate an ISO using GRUB1 in images/<name>.iso [x86, amd64]" 
	@echo "  grub2iso   - Generate an ISO using GRUB2 in images/<name>.iso [x86, amd64]" 
	@echo "  qemu       - Use Qemu to run 'name'." 
	@echo "  exportpack - Export binaries with launch support." 
	@echo "  vbox       - Use VirtualBox to run 'name'." 
	@echo "  fastboot   - Call fastboot with the created rawimage."
	@echo "  fastboot_rawimage - Call fastboot with the created rawimage."
	@echo "  fastboot_uimage   - Call fastboot with the created uimage."
	@echo "  ux         - Run 'name' under Fiasco/UX. [x86]" 
	@echo "  kexec      - Issue a kexec call to start the entry." 
	@echo " Add 'E=name' to directly select the entry without using the menu."
	@echo " Modules are defined in conf/modules.list."

listplatforms: $(KCONFIG_FILE).platforms.list
	$(VERBOSE)sed -nE "s/^\[$(BUILD_ARCH)\](.*)/\1/p" $(KCONFIG_FILE).platforms.list | sort -b


.PHONY: elfimage rawimage uimage qemu vbox ux switch_ram_base \
        grub1iso grub2iso listentries shellcodeentry exportpack \
        fastboot fastboot_rawimage fastboot_uimage \
	check_and_adjust_ram_base listplatforms itb

switch_ram_base:
	$(VERBOSE)$(call switch_ram_base_func,$(RAM_BASE))

check_base_dir:
	@if [ -z "$(CHECK_BASE_DIR)" ]; then                                  \
	  echo "Need to set CHECK_BASE_DIR variable";                         \
	  exit 1;                                                             \
	fi

BID_CHECKBUILD_LOG_REDIR_f = $(if $(BID_CHECKBUILD_LOG), 1>>$(BID_CHECKBUILD_LOG).$(strip $(1)).log) \
			     $(if $(BID_CHECKBUILD_LOG), 2>&1) #>$(BID_CHECKBUILD_LOG).$(strip $(1)).log)

.PRECIOUS: $(CHECK_BASE_DIR)/config.%/.kconfig
.PRECIOUS: $(CHECK_BASE_DIR)/config.%/.config.all
.PHONY: FORCE

checkbuild_prepare.%:
	$(if $(CHECK_INCREMENTAL),,rm -rf $(CHECK_BASE_DIR)/$(patsubst checkbuild_prepare.%,config.%,$@))

$(CHECK_BASE_DIR)/config.%/.kconfig: mk/defconfig/config.% checkbuild_prepare.%
	mkdir -p $(@D)
	cp $< $@

$(CHECK_BASE_DIR)/config.%/.config.all: $(CHECK_BASE_DIR)/config.%/.kconfig FORCE
	find $(@D) -xtype l -delete
	rm -rf $(@D)/pc
	$(MAKE) -j 1 O=$(@D) olddefconfig $(call BID_CHECKBUILD_LOG_REDIR_f, $*)

checkbuild.%: $(CHECK_BASE_DIR)/config.%/.config.all $(CHECK_BASE_DIR)/config.%/.kconfig check_base_dir
	$(MAKE) O=$(<D) BID_CHECKBUILD=1 report $(call BID_CHECKBUILD_LOG_REDIR_f, $*)
	$(MAKE) O=$(<D) BID_CHECKBUILD=1 tool $(call BID_CHECKBUILD_LOG_REDIR_f, $*)
	$(MAKE) O=$(<D) BID_CHECKBUILD=1 USE_CCACHE=$(strip $(USE_CCACHE)) $(CHECK_MAKE_ARGS) $(call BID_CHECKBUILD_LOG_REDIR_f, $*)
	$(VERBOSE)if [ -e $(<D)/ext-pkg ]; then \
	  echo "$(<D)/ext-pkg created. That must not happen in checkbuild."; \
	  exit 1; \
	fi
	$(if $(CHECK_REMOVE_OBJDIR),rm -rf $(<D))

checkbuild: $(if $(USE_CONFIGS),$(addprefix checkbuild.,$(USE_CONFIGS)),$(patsubst mk/defconfig/config.%, checkbuild.%, $(wildcard mk/defconfig/config.*)))


report:
	@echo -e $(EMPHSTART)"============================================================="$(EMPHSTOP)
	@echo -e $(EMPHSTART)" Note, this report might disclose private information"$(EMPHSTOP)
	@echo -e $(EMPHSTART)" Please review (and edit) before making it public"$(EMPHSTOP)
	@echo -e $(EMPHSTART)"============================================================="$(EMPHSTOP)
	@echo
	@echo "make -v:"
	@make -v || true
	@echo
	@echo "CC: $(CC) -v:"
	@$(CC) -v 2>&1 || true
	@echo
	@echo "CXX: $(CXX) -v:"
	@$(CXX) -v 2>&1 || true
	@echo
	@echo "HOST_CC: $(HOST_CC) -v:"
	@$(HOST_CC) -v 2>&1 || true
	@echo
	@echo "HOST_CXX: $(HOST_CXX) -v:"
	@$(HOST_CXX) -v 2>&1 || true
	@echo
	@echo -n "ld: $(LD) -v: "
	@$(LD) -v || true
	@echo
	@echo -n "perl -v:"
	@perl -v || true
	@echo
	@echo -n "python -V: "
	@python -V || true
	@echo
	@echo -n "python2 -V: "
	@python2 -V || true
	@echo
	@echo -n "python3 -V: "
	@python3 -V || true
	@echo
	@echo "svn --version: "
	@svn --version || true
	@echo
	@echo "git --version: "
	@git --version || true
	@echo
	@echo "Shell is:"
	@ls -la /bin/sh || true
	@echo
	@echo "uname -a: "; uname -a
	@echo
	@echo "Distribution"
	@if [ -e "/etc/debian_version" ]; then                 \
	  if grep -qi ubuntu /etc/issue; then                  \
	    echo -n "Ubuntu: ";                                \
	    cat /etc/issue;                                    \
	  else                                                 \
	    echo -n "Debian: ";                                \
	  fi;                                                  \
	  cat /etc/debian_version;                             \
	elif [ -e /etc/gentoo-release ]; then                  \
	  echo -n "Gentoo: ";                                  \
	  cat /etc/gentoo-release;                             \
	elif [ -e /etc/SuSE-release ]; then                    \
	  echo -n "SuSE: ";                                    \
	  cat /etc/SuSE-release;                               \
	elif [ -e /etc/fedora-release ]; then                  \
	  echo -n "Fedora: ";                                  \
	  cat /etc/fedora-release;                             \
	elif [ -e /etc/redhat-release ]; then                  \
	  echo -n "Redhat: ";                                  \
	  cat /etc/redhat-release;                             \
	  if [ -e /etc/redhat_version ]; then                  \
	    echo "  Version: `cat /etc/redhat_version`";       \
	  fi;                                                  \
	elif [ -e /etc/slackware-release ]; then               \
	  echo -n "Slackware: ";                               \
	  cat /etc/slackware-release;                          \
	  if [ -e /etc/slackware-version ]; then               \
	    echo "  Version: `cat /etc/slackware-version`";    \
	  fi;                                                  \
	elif [ -e /etc/mandrake-release ]; then                \
	  echo -n "Mandrake: ";                                \
	  cat /etc/mandrake-release;                           \
	else                                                   \
	  echo "Unknown distribution";                         \
	fi
	@lsb_release -a || true
	@echo
	@echo "Running as PID / User"
	@id || true
	@echo
	@echo Locale info:
	@locale
	@echo
	@echo "Archive information:"
	@echo "SVN:"
	@svn info || true
	@echo "Git:"
	@git describe || true
	@echo
	@echo "CC       = $(CC) $(CCXX_FLAGS)"
	@echo "CXX      = $(CXX) $(CCXX_FLAGS)"
	@echo "HOST_CC  = $(HOST_CC)"
	@echo "HOST_CXX = $(HOST_CXX)"
	@echo "LD       = $(LD)"
	@echo "Paths"
	@echo "Current:   $$(pwd)"
	@echo "L4DIR:     $(L4DIR)"
	@echo "L4DIR_ABS: $(L4DIR_ABS)"
	@echo "OBJ_BASE:  $(OBJ_BASE)"
	@echo "OBJ_DIR:   $(OBJ_DIR)"
	@echo
	@for i in pkg \
	          ../fiasco/src/kern/ia32 \
	          ../fiasco/tool/preprocess/src/preprocess; do \
	  if [ -e $$i ]; then \
	    echo Path $$i found ; \
	  else                \
	    echo PATH $$i IS NOT AVAILABLE; \
	  fi \
	done
	@echo
	@echo Configuration:
	@for i in $(OBJ_DIR)/.config.all $(OBJ_DIR)/.kconfig   \
	          $(OBJ_DIR)/Makeconf.local                    \
	          $(L4DIR_ABS)/Makeconf.local                  \
	          $(OBJ_DIR)/conf/Makeconf.boot                \
	          $(L4DIR_ABS)/conf/Makeconf.boot; do          \
	  if [ -e "$$i" ]; then                                \
	    echo "______start_______________________________:";\
	    echo "$$i:";                                       \
	    cat $$i;                                           \
	    echo "____________________________end___________"; \
	  else                                                 \
	    echo "$$i not found";                              \
	  fi                                                   \
	done
	@echo -e $(EMPHSTART)"============================================================="$(EMPHSTOP)
	@echo -e $(EMPHSTART)" Note, this report might disclose private information"$(EMPHSTOP)
	@echo -e $(EMPHSTART)" Please review (and edit) before making it public"$(EMPHSTOP)
	@echo -e $(EMPHSTART)"============================================================="$(EMPHSTOP)

help::
	@echo
	@echo "Miscellaneous targets:"
	@echo "  switch_ram_base RAM_BASE=0xaddr"
	@echo "                   - Switch physical RAM base of build to 'addr'."
	@echo "  update           - Update working copy by using SVN."
	@echo "  cont             - Continue building after fixing a build error."
	@echo "  clean            - Call 'clean' target recursively."
	@echo "  cleanfast        - Delete all directories created during build."
	@echo "  doc              - Generate documentation."
	@echo "                     The default behavior is building HTML with"
	@echo "                     graphics. To change the default behavior,"
	@echo "                     set DOC_VARIANT to one of the following values:"
	@echo "                     * DOC_VARIANT=fast: Build HTML without graphics."
	@echo "                     * DOC_VARIANT=full: Build HTML and a PDF with graphics."
	@echo "                       The PDF is named doc/source/l4re/latex/refman.pdf."
	@echo "                     * DOC_VARIANT=release: Build everything like 'full'"
	@echo "                       excluding internal information."
	@echo "  report           - Print out host configuration information."
	@echo "  help             - Print this help text."
	@echo "  test             - Run kernel and user-land tests. If 'TEST_KUNIT_DIR' is"
	@echo "                     not provided, only user-land tests run."
	@echo "  listplatforms    - List available platforms."


.PHONY: test
test:
	$(VERBOSE)taparchive="$(TAPARCHIVE)"; \
	if [ -n "$${taparchive%%/*}" ]; then \
	  echo "ERROR: TAPARCHIVE must be an absolute path."; \
	  exit 1; \
	fi
	$(VERBOSE)if [ -z "$(TEST_KUNIT_DIR)" ]; then \
	  echo "INFO: TEST_KUNIT_DIR not provided. No kernel tests."; \
	fi
	$(VERBOSE)test_tmp_dir=$$(mktemp -d); \
	\
	ln -fs "$(OBJ_BASE)/test/t/$(ARCH)_$(CPU)/$(BUILD_ABI)" \
	       "$${test_tmp_dir}/bid-tests"; \
	\
	$(if $(TEST_KUNIT_DIR),\
	  $(L4DIR)/tool/bin/gen_kunit_test --ddir=$${test_tmp_dir}/kunit-tests \
	    --sdir=$(TEST_KUNIT_DIR) --obj-base=$(OBJ_BASE);) \
	\
	(cd $${test_tmp_dir} && \
	 prove $(if $(TAPARCHIVE),-a $(TAPARCHIVE)) $(if $(VERBOSE),,-v) \
	       -m -r $(if $(TEST_KUNIT_DIR),kunit-tests) \
	       "bid-tests/$${TESTS#bid-tests/}"); \
	rm -fr "$${test_tmp_dir}"

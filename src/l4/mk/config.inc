# -*- Makefile -*-
# vim:set ft=make:
# DROPS (Dresden Realtime OPerating System) Component
#
# Makefile-Include for compiling templates (prog.mk, lib.mk)
#

# Makefile-Include for binary, lib, subdir and other directories.
# Definitions and rules for the DROPS configuration tool.

# Supported targets:
#
# config::              - run the menu-driven configuration tool
# menuconfig xconfig::	- run the configuration tool
# oldconfig::	        - (re)create the configuration header
#                         based on a prior configuration
#                         or default values
#
# Required Parameters:
#
# PKGDIR
#
#
# Optional Parameters:
#
# DROPSCONF		- if nonempty, the configuration tool is run for
#			  target config::. If empty, the configuration tool
#			  is not run.
# DROPSCONF_TITLE	- the main title in the configuration tool.
# DROPSCONF_DEFCONFIG	- default config file
# DROPSCONF_CONFIG_IN	- configuration defintion file
# DROPSCONF_CONFIG	- config file
# DROPSCONF_CONFIG_H	- generated config header file
# DROPSCONF_MACRO	- macro to indicate inclusion of config header file
# DROPSCONF_HELPFILE	- options help file
# DROPSCONF_TOOL	- the menudriven configuration tool
# DROPSCONF_TOOL_TXT	- the configuration tool
# DROPSCONF_TOOL_OLD	- helper for recreating the config header file


KCONFIG_OBJ_DIR  = $(OBJ_BASE)/tool/kconfig

kconfig_call = [ -d $@ ] || install -d $(OBJ_BASE)/include/l4; \
	       $(MAKE) -C $(L4DIR)/tool/kconfig O=$(OBJ_BASE) \
               Kconfig=$(KCONFIG_FILE) \
	       KCONFIG_AUTOHEADER=include/l4/bid_config.h \
	       KCONFIG_CONFIG=.kconfig \
	       KCONFIG_AUTOCONFIG=.kconfig.auto \
	       KCONFIG_TRISTATE=.kconfig.tristate \
	       l4re_srcdir=$(L4DIR_ABS)


DROPSCONF		?=
#DROPSCONF_TITLE		?= DROPS Configuration Tool
#DROPSCONF_DEFCONFIG	?= defconfig
#DROPSCONF_CONFIG_IN	?= config.in
#DROPSCONF_CONFIG	?= $(OBJ_DIR)/.config
#DROPSCONF_CONFIG_H	?= $(OBJ_DIR)/config.h
#DROPSCONF_CONFIG_MK	?= $(OBJ_DIR)/Makeconf.bid.local
DROPSCONF_DONTINC_MK	?=
#DROPSCONF_MACRO		?= CONFIG_H_INCLUDED
#DROPSCONF_HELPFILE	?= config.help
#DROPSCONF_LXDIALOG	?= $(OBJ_BASE)/tool/config/lxdialog/lxdialog
#DROPSCONF_TOOL		?= $(firstword $(wildcard \
#					$(L4DIR)/tool/config/Menuconfig \
#					$(DROPS_STDDIR)/tool/bin/Menuconfig) \
#					did_not_find_BID_Menuconfig)
#DROPSCONF_TOOL_TXT	?= $(firstword $(wildcard \
#					$(L4DIR)/tool/config/Configure \
#					$(DROPS_STDDIR)/tool/bin/Configure) \
#					did_not_find_BID_Configure)
#DROPSCONF_TOOL_OLD	?= $(firstword $(wildcard \
#					$(L4DIR)/tool/config/Configure \
#					$(DROPS_STDDIR)/tool/bin/Configure) \
#					did_not_find_BID_Configure) -d
#DROPSCONF_VARDEFS = $(foreach v,TITLE DEFCONFIG CONFIG_IN CONFIG CONFIG_H \
#	MACRO HELPFILE UNDEF LXDIALOG,DROPSCONF_$v='$(DROPSCONF_$v)')

ifneq ($(DROPSCONF),)
.o:	$(DROPSCONF_CONFIG_H)

DIRS_FOR_CONFIG = $(KCONFIG_OBJ_DIR) $(OBJ_BASE)/config

$(DIRS_FOR_CONFIG): $(KCONFIG_FILE)
	$(VERBOSE)[ -d $@ ] || install -d $@

menuconfig config: $(DIRS_FOR_CONFIG)
	$(VERBOSE)+$(kconfig_call) menuconfig syncconfig
	$(VERBOSE)test ! -r $(DROPSCONF_CONFIG) -o \
		! $(DROPSCONF_CONFIG) -nt $(DROPSCONF_CONFIG_MK) || \
		$(MAKE) $(DROPSCONF_CONFIG_MK)

textconfig: $(DIRS_FOR_CONFIG)
	$(VERBOSE)+$(kconfig_call) config syncconfig
	@$(MAKE) $(DROPSCONF_CONFIG_MK)

oldconfig olddefconfig xconfig gconfig nconfig randconfig allyesconfig allnoconfig savedefconfig: $(DIRS_FOR_CONFIG)
	$(VERBOSE)+$(kconfig_call) $@ syncconfig
	@$(MAKE) $(DROPSCONF_CONFIG_MK)

$(DROPSCONF_CONFIG) syncconfig: $(DIRS_FOR_CONFIG)
	$(VERBOSE)+$(kconfig_call) syncconfig
	

#$(DROPSCONF_CONFIG_H): $(DROPSCONF_CONFIG)

#$(DROPSCONF_CONFIG_H) $(DROPSCONF_CONFIG): $(DROPSCONF_CONFIG_IN)
#	@$(GEN_MESSAGE)
#	$(VERBOSE)install -d $(dir $(DROPSCONF_CONFIG))
#	$(VERBOSE)install -d $(dir $(DROPSCONF_CONFIG_H))
#	$(VERBOSE)if tty >/dev/null; then \
#	  $(DROPSCONF_VARDEFS) $(DROPSCONF_TOOL_OLD); \
#	 else \
#	  true | $(DROPSCONF_VARDEFS) $(DROPSCONF_TOOL_OLD) \
#	  $(if $(VERBOSE),>/dev/null,) || \
#	  ( echo -e "\nError: Unattended mode -- Some defaults for config options are missing." ; \
#	    false ) \
#	 fi

$(DROPSCONF_CONFIG_MK): $(DROPSCONF_CONFIG)
	$(VERBOSE)sed -e "s/\(^[^= ]*=\)'\([^']*\)'/\1\2/" \
		<$(DROPSCONF_CONFIG) >$@
	$(VERBOSE)$(MAKE) DROPSCONF_CONFIG_MK_POST_HOOK

DROPSCONF_CONFIG_MK_POST_HOOK::
	
#config:: $(DROPSCONF_LXDIALOG)
#	$(VERBOSE)install -d $(dir $(DROPSCONF_CONFIG_H))
#	$(VERBOSE)$(DROPSCONF_VARDEFS) $(DROPSCONF_TOOL)
#	$(VERBOSE)test ! -r $(DROPSCONF_CONFIG) -o \
#		! $(DROPSCONF_CONFIG) -nt $(DROPSCONF_CONFIG_MK) || \
#		$(MAKE) $(DROPSCONF_CONFIG_MK)

#txtconfig::
#	$(VERBOSE)install -d $(dir $(DROPSCONF_CONFIG_H))
#	$(VERBOSE)$(DROPSCONF_VARDEFS) $(DROPSCONF_TOOL_TXT)
#	@$(MAKE) $(DROPSCONF_CONFIG_MK)
#
#oldconfig::
#	$(VERBOSE)install -d $(dir $(DROPSCONF_CONFIG_H))
#	$(VERBOSE)$(DROPSCONF_VARDEFS) $(DROPSCONF_TOOL_OLD)
#	@$(MAKE) $(DROPSCONF_CONFIG_MK)
#
#$(DROPSCONF_LXDIALOG):
#	$(VERBOSE)install -d $(@D)
#	$(VERBOSE)PWD=$(call absfilename,$(L4DIR)/tool/config) $(MAKE) -C $(L4DIR)/tool/config
#
clean::

ifeq ($(S),)
cleanall::
	$(VERBOSE)$(RM) $(DROPSCONF_CONFIG) $(DROPSCONF_CONFIG_H) \
			$(DROPSCONF_CONFIG_MK) .menuconfig.log \
			$(DROPSCONF_CONFIG).old
endif

help::
	@echo
	@echo "Configuration targets:"
	@echo "  config           - run the menu-driven configuration tool"
	@echo "  gconfig          - run a graphical configuration tool"
	@echo "  xconfig          - run a graphical configuration tool"
	@echo "  oldconfig        - (re)create the configuration header based on a prior"
	@echo "                     configuration or default values"

# special switch not to include DROPSCONF_CONFIG_MK
ifeq ($(DROPSCONF_DONTINC_MK),)
# do not prebuild the config file on "make config"
ifeq ($(filter config oldconfig syncconfig txtconfig help scrub clean cleanall $(DROPSCONF_CONFIG_MK),$(MAKECMDGOALS)),)
$(info INCLUDING $(DROPSCONF_CONFIG_MK) config.inc)
-include $(DROPSCONF_CONFIG_MK)
endif
endif

# end of DROPSCONF defined
else
config txtconfig oldconfig olddefconfig syncconfig::
endif

.PHONY: config oldconfig olddefconfig syncconfig txtconfig menuconfig \
        xconfig gconfig nconfig randconfig allyesconfig allnoconfig \
	savedefconfig

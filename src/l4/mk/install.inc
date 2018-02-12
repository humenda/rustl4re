# -*- Makefile -*-
# vim:set ft=make:
#
# DROPS (Dresden Realtime OPerating System) Component
#
# Makefile-Include for install-rules
#
# This file is useful only when actually building files, thus if
# BUILD_ONE_ARCHITECTURE (see Makeconf) is defined.
# INSTALL_TARGET and INSTALL_TARGET_$(ARCH) contain the targets to be
# installed. If they are both empty, and NOTARGETSTOINSTALL is not set,
# they are set to TARGET and TARGET_$(ARCH).
#
# INSTALLDIR and INSTALLDIR_LOCAL is the dir the files will be installed
# in and must be set prior to including this file. INSTALLDIR is used by the
# target "install" and "INSTALLDIR_LOCAL" by the target "all".
#
# The target "all" is made depending on the installed files in
# INSTALLDIR_LOCAL. "install" on the files in INSTALLDIR. Another
# difference is that "all" results in using the variable
# $(INSTALLFILE_LOCAL) to install the files and "install" in using
# $(INSTALLFILE). This allows the difference between linking and actually
# copying (with stripping for bins).

ifndef INSTALL_TARGET_$(OSYSTEM)
ifndef NOTARGETSTOINSTALL
INSTALL_TARGET?=$(TARGET)
endif
endif
INSTALL_TARGET += $(INSTALL_TARGET_$(OSYSTEM))

all::	$(addprefix $(INSTALLDIR_LOCAL)/,$(INSTALL_TARGET))
install:: $(addprefix $(INSTALLDIR)/,$(INSTALL_TARGET))

# with prog.mk, we copy
ifeq ($(ROLE),prog.mk)
$(foreach f,$(INSTALL_TARGET),$(INSTALLDIR_LOCAL)/$(f)):$(INSTALLDIR_LOCAL)/%:%
	@$(INSTALL_LOCAL_MESSAGE)
	$(if $(INSTALLFILE_LOCAL),$(call create_dir,$(dir $@)))
	$(VERBOSE)$(call INSTALLFILE_LOCAL,$<,$@)
else
# with others, we symlink
# we check for each locally installed lib if the symlink is correct or if
# it's a shared lib since we need to always call the local install rule as
# we need to load shared libs to the target
.PHONY: $(addprefix $(INSTALLDIR_LOCAL)/,$(INSTALL_TARGET))
$(foreach f,$(INSTALL_TARGET),$(INSTALLDIR_LOCAL)/$(f)):$(INSTALLDIR_LOCAL)/%:%
	@$(if $(INSTALLFILE_LOCAL), \
		f="$@"; \
		if [ ! $< -ef $@ -o "$${f%%.so}" != "$$f" ] ; then \
		  $(if $(INSTALL_LOCAL_MESSAGE),$(INSTALL_LOCAL_MESSAGE);) \
		  $(INSTALL) -d $(dir $@); \
		  $(call INSTALLFILE_LOCAL,$<,$@); \
		fi)
endif

$(foreach f,$(INSTALL_TARGET),$(INSTALLDIR)/$(f)):$(INSTALLDIR)/%:%
	@$(INSTALL_MESSAGE)
	$(if $(INSTALLFILE),$(call create_dir,$(dir $@)))
	$(VERBOSE)$(call INSTALLFILE,$<,$@)

# unconditionally install on "make install"
.PHONY: $(addprefix $(INSTALLDIR)/,$(INSTALL_TARGET))

.general.d: $(L4DIR)/mk/install.inc


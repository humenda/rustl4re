# -*- Makefile -*-

ifeq ($(origin _L4DIR_MK_ASSETS_MK),undefined)
_L4DIR_MK_ASSETS_MK=y

ROLE = assets.mk

include $(L4DIR)/mk/Makeconf
$(GENERAL_D_LOC): $(L4DIR)/mk/assets.mk

define assets_subdir
  $(1)$(if $(ASSET_TYPE),/$(ASSET_TYPE))
endef

INSTALLDIR_ASSETS        ?= $(call assets_subdir,$(DROPS_STDDIR)/assets)
INSTALLDIR_ASSETS_LOCAL  ?= $(call assets_subdir,$(OBJ_BASE)/assets)
INSTALLFILE_ASSETS       ?= $(INSTALL) -m 644 $(1) $(2)
INSTALLFILE_ASSETS_LOCAL ?= $(LN) -sf $(call absfilename,$(1)) $(2)

INSTALLFILE               = $(INSTALLFILE_ASSETS)
INSTALLDIR                = $(INSTALLDIR_ASSETS)
INSTALLFILE_LOCAL         = $(INSTALLFILE_ASSETS_LOCAL)
INSTALLDIR_LOCAL          = $(INSTALLDIR_ASSETS_LOCAL)

MODE                     ?= assets

REQUIRE_HOST_TOOLS       ?= $(if $(SRC_DTS),dtc)

include $(L4DIR)/mk/binary.inc

ifneq ($(SYSTEM),) # if we are a system, really build

# Functionality for device-tree file handling
ifeq ($(ASSET_TYPE),dtb)
TARGET_DTB  = $(patsubst %.dts,%.dtb,$(SRC_DTS))
TARGET     += $(TARGET_DTB)
DEPS       += $(foreach file,$(TARGET_DTB),$(dir $(file)).$(notdir $(file)).d)
endif

include $(L4DIR)/mk/install.inc

endif # SYSTEM

.PHONY: all clean cleanall config help install oldconfig txtconfig
-include $(DEPSVAR)
help::
	@echo "  all            - generate assets locally"
ifneq ($(SYSTEM),)
	@echo "                   to $(INSTALLDIR_LOCAL)"
endif
	@echo "  install        - generate and install assets globally"
ifneq ($(SYSTEM),)
	@echo "                   to $(INSTALLDIR)"
endif
	@echo "  scrub          - delete backup and temporary files"
	@echo "  clean          - delete generated object files"
	@echo "  cleanall       - delete all generated, backup and temporary files"
	@echo "  help           - this help"

endif # _L4DIR_MK_ASSETS_MK undefined

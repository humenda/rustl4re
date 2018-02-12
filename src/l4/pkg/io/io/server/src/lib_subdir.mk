ifneq ($(SYSTEM),)
PRIVATE_INCDIR     += $(PKGDIR)/server/src

# do not generate PC files for this lib
PC_FILENAMES       :=
TARGET             := builtin.thin.a
NOTARGETSTOINSTALL := 1
SUBDIRS            += $(SUBDIRS_$(ARCH)) $(SUBDIRS_$(OSYSTEM))
SUBDIR_TARGETS     := $(addsuffix /OBJ-$(SYSTEM)/$(TARGET),$(SUBDIRS))
SUBDIR_OBJS         = $(addprefix $(OBJ_DIR)/,$(SUBDIR_TARGETS))
OBJS_$(TARGET)     += $(SUBDIR_OBJS)

# the all target must be first!
all::

# our bultin.a dependency
$(TARGET): $(SUBDIR_OBJS) $(PKGDIR)/server/src/Makefile.config

# Make.rules is here as it contains the config what to
# include
$(SUBDIR_OBJS): $(OBJ_DIR)/%/OBJ-$(SYSTEM)/$(TARGET): % $(PKGDIR)/server/src/Make.rules $(PKGDIR)/server/src/Makefile.config
	$(VERBOSE)$(MAKE) $(MAKECMDGOALS) OBJ_BASE=$(OBJ_BASE) \
                          -C $(SRC_DIR)/$* $(MKFLAGS)
endif

all::

clean-subdir-%:
	$(VERBOSE)$(MAKE) clean OBJ_BASE=$(OBJ_BASE) \
                          -C $(SRC_DIR)/$* $(MKFLAGS)

clean:: $(addprefix clean-subdir-,$(SUBDIRS))

include $(L4DIR)/mk/lib.mk

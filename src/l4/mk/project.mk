include $(L4DIR)/mk/Makeconf
BID_PRJ_DIR_MAX_DEPTH ?= 4

ifneq ($(S),)
# handle explicit subdir builds here

comma := ,
ALL_SUBDIRS   := $(subst :, ,$(subst $(comma), ,$(S)))

BID_DCOLON_TARGETS := all clean cleanall install scrub DROPSCONF_CONFIG_MK_POST_HOOK

all:: $(ALL_SUBDIRS)

.PHONY: $(ALL_SUBDIRS) FORCE

$(ALL_SUBDIRS):%: FORCE
	@$(PKG_MESSAGE)
	$(VERBOSE)$(MAKE) S= -C $(SRC_DIR)/$@ $(MAKECMDGOALS)

$(filter-out $(BID_DCOLON_TARGETS),$(MAKECMDGOALS)): $(ALL_SUBDIRS)
$(filter $(BID_DCOLON_TARGETS),$(MAKECMDGOALS)):: $(ALL_SUBDIRS)

else
# handle normal targets


find_prj_dirs = $(shell $(L4DIR)/mk/pkgfind $(1) $(BID_PRJ_DIR_MAX_DEPTH))

# all our packages
ifeq ($(ALL_SUBDIRS),)
  ifeq ($(PRJ_SUBDIRS),)
    ALL_SUBDIRS := $(call find_prj_dirs, $(SRC_DIR))
  else
    ALL_SUBDIRS := $(foreach d, $(PRJ_SUBDIRS), $(addprefix $d/,$(call find_prj_dirs, $(SRC_DIR)/$(d))))
  endif
endif

# the broken packages
BROKEN_SUBDIRS	= $(patsubst %/broken, %, \
			$(wildcard $(addsuffix /broken,$(ALL_SUBDIRS))))
# the obsolete packages
OBSOLETE_SUBDIRS = $(patsubst %/obsolete, %, \
			$(wildcard $(addsuffix /obsolete,$(ALL_SUBDIRS))))
# and the packages we are supposed to build
BUILD_SUBDIRS  = $(filter-out $(BROKEN_SUBDIRS) $(OBSOLETE_SUBDIRS) $(FILTER_OUT_SUBDIRS), \
                   $(ALL_SUBDIRS))

# force that every package to be built has a Control and Makefile
ifneq ($(addsuffix /Makefile,$(BUILD_SUBDIRS)),$(wildcard $(addsuffix /Makefile,$(BUILD_SUBDIRS))))
$(error Missing Makefile files: $(filter-out $(wildcard $(addsuffix /Makefile,$(BUILD_SUBDIRS))), $(addsuffix /Makefile,$(BUILD_SUBDIRS))))
endif

ifneq ($(addsuffix /Control,$(BUILD_SUBDIRS)),$(wildcard $(addsuffix /Control,$(BUILD_SUBDIRS))))
$(error Missing Control files: $(filter-out $(wildcard $(addsuffix /Control,$(BUILD_SUBDIRS))), $(addsuffix /Control,$(BUILD_SUBDIRS))))
endif

BID_STATE_FILE = $(OBJ_DIR)/BUILD.state
BID_SAFE_STATE = @echo 'BID_STATE_DONE+=$@' >> $(BID_STATE_FILE)

# a working heuristic
PRJ_DIRS := \
  $(patsubst ./%,%, \
    $(patsubst %/Makefile,%, \
      $(shell cd $(SRC_DIR) && find . -maxdepth  $(BID_PRJ_DIR_MAX_DEPTH) -type f -name Makefile \
	      | xargs grep -l -E '^\s*include\s+\$$\(L4DIR\)/mk/project.mk')))

# given order matters
ALIASES_DIRS = $(wildcard $(L4DIR)/mk/aliases.d) \
               $(wildcard $(OBJ_BASE)/aliases.d) \
               $(wildcard $(addsuffix /aliases.d,$(shell find $(SRC_DIR) -maxdepth $(BID_PRJ_DIR_MAX_DEPTH) -type d -name prj-config)))

# all package config files go here
PKGCONF_DIR  = $(OBJ_BASE)/pc

# the default command for generating package dependencies
PKGDEPS_CMD  = $(L4DIR)/mk/pkgdeps $(PKGDEPS_IGNORE_MISSING) \
                     -P $(PKGCONF_DIR) $(addprefix -A ,$(ALIASES_DIRS))
PKGDEPS_IGNORE_MISSING := -I

PKG_MESSAGE       =echo "=== Building package \"$(basename $@)\" ==="
DOC_MESSAGE       =echo "=== Creating documentation for package \"$(1)\" ==="
INST_MESSAGE      =echo "=== Installing Package \"$(1)\" ==="

PRJ_DIRS_Makfiles = $(addsuffix /Makefile,$(addprefix $(OBJ_DIR)/,$(PRJ_DIRS)))

$(PRJ_DIRS_Makfiles):$(OBJ_BASE)/%/Makefile:
	$(call build_obj_redir_Makefile,$@,$(SRC_BASE_ABS)/$*)

all:: $(PRJ_DIRS_Makfiles) $(BUILD_SUBDIRS)

ifeq ($(MAKECMDGOALS),)
  $(BUILD_SUBDIRS): BID_cont_reset
endif

-include $(BID_STATE_FILE)
.PHONY: BID_cont_reset cont

BID_cont_reset:
	$(if $(filter cont,$(MAKECMDGOALS)),,$(VERBOSE)$(RM) $(BID_STATE_FILE))

cont:
	$(VERBOSE)$(MAKE) $(addprefix -o ,BID_cont_reset $(BID_STATE_DONE)) \
		$(filter-out cont,$(MAKECMDGOALS))
	$(if $(BID_POST_CONT_HOOK),$(VERBOSE)$(BID_POST_CONT_HOOK))

$(PKGCONF_DIR):
	$(VERBOSE)mkdir -p $(PKGCONF_DIR)

$(OBJ_DIR)/.Package.deps.pkgs: FORCE
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)echo $(BUILD_SUBDIRS) > $@.tmp
	$(VERBOSE)$(call move_if_changed,$@,$@.tmp)

# deps on disappearing aliases.d-files are not handled...
$(OBJ_DIR)/.Package.deps: $(L4DIR)/mk/pkgdeps $(OBJ_DIR)/.Package.deps.pkgs \
                          $(if $(filter update up,$(MAKECMDGOALS)),,Makefile) \
                          $(wildcard $(foreach d,$(ALIASES_DIRS),$(d)/*)) \
                          $(wildcard $(foreach d,$(BUILD_SUBDIRS),$(d)/Control)) \
                          $(PKGCONF_DIR)
	$(VERBOSE)$(PKGDEPS_CMD) generate $(SRC_DIR) $(ALL_SUBDIRS) > $@.tmp
	$(VERBOSE)$(call move_if_changed,$@,$@.tmp)

include $(OBJ_DIR)/.Package.deps

$(ALL_SUBDIRS):%:%/Makefile BID_cont_reset
	@$(PKG_MESSAGE)
	$(VERBOSE)PWD=$(PWD)/$@ $(MAKE) -C $@ all
	$(VERBOSE)$(BID_SAFE_STATE)

install::
	$(VERBOSE)set -e; for i in $(BUILD_SUBDIRS); do \
	  $(call INST_MESSAGE,$(BID_DOLLARQUOTE)$$i); \
	  PWD=$(PWD)/$$i $(MAKE) -C $$i $@; \
	done

clean cleanall:: BID_cont_reset
	$(VERBOSE)$(RM) $(UPDATE_LOG)
	$(VERBOSE)for i in $(BUILD_SUBDIRS) $(OBSOLETE_SUBDIRS); do \
	  echo "=== Cleaning in package  \"$$i\" ==="; \
	  if [ -r $$i/Makefile ] ; then PWD=$(PWD)/$$i $(MAKE) -C $$i $@; fi ; \
	done

# disable the 'doc' target:
# This conficts with the top-level 'doc'target and it seems
# to be unused for quite a while
ifeq (a,b)
doc:
	$(VERBOSE)set -e; for i in $(BUILD_SUBDIRS); do \
	  if [ -e $(PWD)/$$i/doc ]; then                \
	  $(call DOC_MESSAGE,$(BID_DOLLARQUOTE)$$i);    \
	  PWD=$(PWD)/$$i $(MAKE) -C $$i $@;             \
	  fi;                                           \
	done
endif

print-subdirs:
	@echo $(BUILD_SUBDIRS)
	@echo obsolete: $(OBSOLETE_SUBDIRS)
	@echo broken: $(BROKEN_SUBDIRS)

.PHONY: help
help::
	@echo "Specify one of the following targets:"
	@echo "all             - build the packages neither broken nor obsolete"
	@echo " Use make S=dir1:dir2:..: to build those directories"
	@echo "cont            - after correcting errors, continue where \"make all\" failed"
	@echo "<pkgname>       - Build package and all its dependencies"
#	@echo "doc             - build documentation (not build per default)"
	@echo "install         - install the packages, use only after making all!"
	@echo
	@echo "clean           - clean the packages"
	@echo "cleanall        - clean the packages pedanticly"
	@echo "print-subdirs   - print the SUBDIRS which are subject to build"
	@echo "BID_cont_reset  - reset the state used by the \"cont\" target"
	@echo "help            - this help"


.PHONY: install clean cleanall doc
.PHONY: $(BUILD_SUBDIRS)

endif

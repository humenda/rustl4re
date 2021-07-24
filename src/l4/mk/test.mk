# -*- Makefile -*-
#
# DROPS (Dresden Realtime OPerating System) Component
#
# Makefile-Template for test directories
#
# See doc/source/build_system.dox for further documentation
#
# Makeconf is used, see there for further documentation
# binary.inc is used, see there for further documentation

ifeq ($(origin _L4DIR_MK_TEST_MK),undefined)
_L4DIR_MK_TEST_MK=y

# Helper for qemu's smp flag
ifneq ($(filter $(ARCH),x86 amd64),)
qemu_smp = -smp $(1),cores=$(1)
else
qemu_smp = -smp $(1)
endif

# auto-fill TARGET with builds for test_*.c[c] if necessary
# TARGETS_$(ARCH) - contains a list of tests specific for this architecture
ifndef TARGET
TARGETS_CC := $(patsubst $(SRC_DIR)/%.cc,%,$(wildcard $(SRC_DIR)/test_*.cc))
$(foreach t, $(TARGETS_CC), $(eval SRC_CC_$(t) += $(t).cc))
TARGETS_C := $(patsubst $(SRC_DIR)/%.c,%,$(wildcard $(SRC_DIR)/test_*.c))
$(foreach t, $(TARGETS_C), $(eval SRC_C_$(t) += $(t).c))
TARGET += $(TARGETS_CC) $(TARGETS_C) $(TARGETS_$(ARCH))
endif

MODE ?= shared
ROLE = test.mk

include $(L4DIR)/mk/Makeconf

# define INSTALLDIRs prior to including install.inc, where the install-
# rules are defined.
ifeq ($(MODE),host)
INSTALLDIR_BIN_LOCAL    = $(OBJ_BASE)/test/bin/host/$(TEST_GROUP)
INSTALLDIR_TEST_LOCAL   = $(OBJ_BASE)/test/t/host/$(TEST_GROUP)
else
INSTALLDIR_BIN_LOCAL    = $(OBJ_BASE)/test/bin/$(subst -,/,$(SYSTEM))/$(TEST_GROUP)
INSTALLDIR_TEST_LOCAL   = $(OBJ_BASE)/test/t/$(subst -,/,$(SYSTEM))/$(TEST_GROUP)
endif

$(GENERAL_D_LOC): $(L4DIR)/mk/test.mk $(SRC_DIR)/Makefile

ifneq ($(SYSTEM),) # if we have a system, really build

# There are two kind of targets:
#  TARGET - contains binary targets that actually need to be built first
#  EXTRA_TEST - contains tests where only test scripts are created
$(foreach t, $(TARGET) $(EXTRA_TEST), $(eval TEST_SCRIPTS += $(t).t))
$(foreach t, $(TARGET) $(EXTRA_TEST), $(eval TEST_TARGET_$(t) ?= $(t)))

ifeq ($(MODE),host)
# in host mode the script can be run directly
test_script = $(INSTALLDIR_BIN_LOCAL)/$(TEST_TARGET_$(1)) "$$@"
else
# all other modes require a VM or similar to be set up
test_script = $(L4DIR)/tool/bin/run_test
endif

# L4RE_ABS_SOURCE_DIR_PATH is used in gtest-internal.h to shorten absolute path
# names to L4Re relative paths.
CPPFLAGS += -DL4RE_ABS_SOURCE_DIR_PATH='"$(L4DIR_ABS)"'

# variables that are forwarded to the test runner environment
testvars_fix    := MODE ARCH NED_CFG REQUIRED_MODULES KERNEL_CONF L4LINUX_CONF \
                    TEST_TARGET TEST_SETUP TEST_EXPECTED TEST_TAGS OBJ_BASE \
                    TEST_ROOT_TASK TEST_DESCRIPTION TEST_KERNEL_ARGS
testvars_conf   := TEST_TIMEOUT TEST_EXPECTED_REPEAT
testvars_append := QEMU_ARGS MOE_ARGS TEST_ROOT_TASK_ARGS BOOTSTRAP_ARGS

# use either a target-specific value or the general version of a variable
targetvar = $(or $($(1)_$(2)),$($(1)))

# This is the same as INSTALLFILE_LIB_LOCAL
INSTALLFILE_TEST_LOCAL = $(LN) -sf $(call absfilename,$(1)) $(2)
DEFAULT_TEST_STARTER = $(L4DIR)/tool/bin/default-test-starter

$(TEST_SCRIPTS):%.t: $(GENERAL_D_LOC)
	$(VERBOSE)echo -e "#!/bin/bash\n\nset -a" > $@
	$(VERBOSE)echo 'L4DIR="$(L4DIR)"' >> $@
	$(VERBOSE)echo 'SEARCHPATH="$(if $(PRIVATE_LIBDIR),$(PRIVATE_LIBDIR):)$(INSTALLDIR_BIN_LOCAL):$(OBJ_BASE)/bin/$(ARCH)_$(CPU):$(OBJ_BASE)/bin/$(ARCH)_$(CPU)/$(BUILD_ABI):$(OBJ_BASE)/lib/$(ARCH)_$(CPU):$(OBJ_BASE)/lib/$(ARCH)_$(CPU)/$(BUILD_ABI):$(SRC_DIR):$(L4DIR)/conf/test"' >> $@
	$(VERBOSE)$(foreach v,$(testvars_fix), echo '$(v)="$(call targetvar,$(v),$(notdir $*))"' >> $@;)
	$(VERBOSE)$(foreach v,$(testvars_conf), echo ': $${$(v):=$(call targetvar,$(v),$(notdir $*))}' >> $@;)
	$(VERBOSE)$(foreach v,$(testvars_append), echo '$(v)="$$$(v) $(call targetvar,$(v),$(notdir $*))"' >> $@;)
	$(VERBOSE)echo ': $${BID_L4_TEST_HARNESS_ACTIVE:=1}' >> $@
	$(VERBOSE)echo 'TEST_TESTFILE="$$0"' >> $@
	$(VERBOSE)echo ': $${TEST_STARTER:=$(DEFAULT_TEST_STARTER)}' >> $@
	$(VERBOSE)echo 'set +a' >> $@
	$(VERBOSE)echo 'exec $$TEST_STARTER "$$@"' >> $@
	$(VERBOSE)chmod 755 $@
	@$(BUILT_MESSAGE)
	@$(call INSTALL_LOCAL_MESSAGE,$@)

# Calculate the list of installed .t files
TEST_SCRIPTS_INST := $(foreach t,$(TEST_SCRIPTS), $(INSTALLDIR_TEST_LOCAL)/$(notdir $(t)))

# Add a dependency for them
all:: $(TEST_SCRIPTS_INST)

# Install rule for the .t files
$(TEST_SCRIPTS_INST):$(INSTALLDIR_TEST_LOCAL)/%: %
	$(VERBOSE)$(call create_dir,$(INSTALLDIR_TEST_LOCAL))
	$(VERBOSE)$(call INSTALLFILE_TEST_LOCAL,$<,$@)

endif	# SYSTEM is defined, really build

include $(L4DIR)/mk/prog.mk
ROLE = test.mk

clean cleanall::
	$(RM) $(TEST_SCRIPTS)

endif	# _L4DIR_MK_TEST_MK undefined

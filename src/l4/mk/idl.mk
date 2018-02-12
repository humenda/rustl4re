# -*- Makefile -*-
#
# DROPS (Dresden Realtime OPerating System) Component
#
# Makefile-Template for idl directories
#
# install.inc is used, see there for further documentation
#		- from install.inc, we use the feature to install files
#		  to the local install directory on 'all' target.
#		  Therefore, we set INSTALLDIR_LOCAL and INSTALLFILE_LOCAL.
#		  Note that we changed INSTALLDIR_IDL_LOCAL AND INSTALLDIR_IDL
# binary.inc is used, see there for further documentation

ifeq ($(origin _L4DIR_MK_IDL_MK),undefined)
_L4DIR_MK_IDL_MK=y

ROLE = idl.mk

DICE_REQ := --require="3.2.0"

# define LOCAL_INSTALLDIR prior to including install.inc, where the install-
# rules are defined. Same for INSTALLDIR.
INSTALLDIR_IDL		?= $(DROPS_STDDIR)/include/$(ARCH)/$(L4API)/$(INSTALL_INC_PREFIX)
INSTALLDIR_IDL_LOCAL	?= $(OBJ_BASE)/include/$(ARCH)/$(L4API)/$(INSTALL_INC_PREFIX)
INSTALLFILE_IDL		?= $(INSTALL) -m 644 $(1) $(2)
INSTALLFILE_IDL_LOCAL	?= $(LN) -sf $(call absfilename,$(1)) $(2)

INSTALLFILE		= $(INSTALLFILE_IDL)
INSTALLDIR		= $(INSTALLDIR_IDL)
INSTALLFILE_LOCAL	= $(INSTALLFILE_IDL_LOCAL)
INSTALLDIR_LOCAL	= $(INSTALLDIR_IDL_LOCAL)

INSTALL_INC_PREFIX     ?= l4/$(PKGNAME)

# our default MODE is 'static'
MODE			?= static

IDL_EXPORT_STUB ?= %
IDL_EXPORT_IDL	?= %

include $(L4DIR)/mk/Makeconf
include $(L4DIR)/mk/binary.inc
$(GENERAL_D_LOC): $(L4DIR)/mk/idl.mk

ifneq ($(SYSTEM),) # if we have a system, really build
#######################################################
#
# SYSTEM valid, we are in an OBJ-<xxx> system subdir
#
#######################################################

IDL_EXPORT_STUB ?= %
IDL_TYPE	?= dice

# C++ specific rules
IDL_C		= c
IDL_H		= h
IDL_CPP		= $(CC)
ifneq (,$(findstring BmCPP,$(IDL_FLAGS)))
IDL_C		= cc
IDL_H		= hh
IDL_CPP		= $(CXX)
else
ifneq (,$(findstring Bmcpp,$(IDL_FLAGS)))
IDL_C		= cc
IDL_H		= hh
IDL_CPP		= $(CXX)
endif
endif

IDL_DEP =		$(addprefix .,$(addsuffix .d,$(notdir $(IDL))))
IDL_SKELETON_C =	$(IDL:.idl=-server.$(IDL_C))
IDL_SKELETON_H =	$(IDL_SKELETON_C:.$(IDL_C)=.$(IDL_H))
IDL_STUB_C =		$(IDL:.idl=-client.$(IDL_C))
IDL_STUB_H = 		$(IDL_STUB_C:.$(IDL_C)=.$(IDL_H))
IDL_OPCODE_H =		$(IDL:.idl=-sys.$(IDL_H))

IDL_FILES = $(IDL_SKELETON_C) $(IDL_SKELETON_H) $(IDL_STUB_C) $(IDL_STUB_H) \
	    $(IDL_OPCODE_H)

# Makro that expands to the list of generated files
# arg1 - name of the idl file. Path and extension will be stripped
IDL_FILES_EXPAND =	$(addprefix $(notdir $(basename $(1))),-server.$(IDL_C) -server.$(IDL_H) -client.$(IDL_C) -client.$(IDL_H) -sys.$(IDL_H))

INSTALL_TARGET = $(patsubst %.idl,%-sys.$(IDL_H),		\
		   $(filter $(IDL_EXPORT_SKELETON) $(IDL_EXPORT_STUB),$(IDL)))\
		 $(patsubst %.idl,%-server.$(IDL_H),		\
		   $(filter $(IDL_EXPORT_SKELETON),$(IDL)))		\
		 $(patsubst %.idl,%-client.$(IDL_H),		\
		   $(filter $(IDL_EXPORT_STUB), $(IDL)))

all:: $(IDL_FILES)
.DELETE_ON_ERROR:

# the dependencies for the generated files
DEPS		+= $(IDL_DEP)

ifneq (,$(filter-out corba dice, $(IDL_TYPE)))
$(error IDL_TYPE is neither <dice> nor <corba>)
endif

# the IDL file is found one directory up
vpath %.idl $(SRC_DIR)

# DICE mode
IDL_FLAGS	+= $(addprefix -P,$(CPPFLAGS))
IDL_FLAGS	+= $(IDL_FLAGS_$(<F))

ifeq ($(L4API),l4f)
IDL_FLAGS	+= -Bifiasco
endif

ifeq ($(L4API),l4v4)
IDL_FLAGS	+= -Biv4
endif

ifeq ($(L4API),l4x2)
IDL_FLAGS	+= -Bix2
endif

ifeq ($(L4API),linux)
IDL_FLAGS	+= -Bisock
endif

ifeq ($(L4API),l4secv2emu)
IDL_FLAGS	+= -fforce-c-bindings -P-DL4API_l4f
endif

ifeq ($(ARCH),x86)
IDL_FLAGS	+= -Bpia32
endif

ifeq ($(ARCH),arm)
IDL_FLAGS	+= -Bparm
endif

ifeq ($(ARCH),amd64)
IDL_FLAGS	+= -Bpamd64
endif

ifeq ($(IDL_TYPE),corba)
IDL_FLAGS	+= --x=corba
endif

# We don't use gendep for generating the dependencies because gendep can only
# catch open() calls from _one_ applications. Here two applications, dice and
# the preprocessor perform open.
%-server.$(IDL_C) %-server.$(IDL_H) %-client.$(IDL_C) %-client.$(IDL_H) %-sys.$(IDL_H): %.idl .general.d
	@$(GEN_MESSAGE)
	$(VERBOSE)CC="$(IDL_CPP)" $(DICE) $(DICE_REQ) $(IDL_FLAGS) -MD $<
	$(VERBOSE)mv $*.d .$(<F).d
	$(DEPEND_VERBOSE)$(ECHO) "$(call IDL_FILES_EXPAND,$<): $(DICE) $<" >>.$(<F).d
	$(DEPEND_VERBOSE)$(ECHO) "$(DICE) $<:" >>.$(<F).d


clean cleanall::
	$(VERBOSE)$(RM) $(wildcard $(addprefix $(INSTALLDIR_LOCAL)/, $(IDL_FILES)))
	$(VERBOSE)$(RM) $(wildcard $(IDL_FILES))

# include install.inc to define install rules
include $(L4DIR)/mk/install.inc

else
#####################################################
#
# No SYSTEM defined, we are in the idl directory
#
#####################################################

# we install the IDL-files specified in IDL_EXPORT_IDL
INSTALL_TARGET = $(filter $(IDL_EXPORT_IDL), $(IDL))

# include install.inc to define install rules
include $(L4DIR)/mk/install.inc

# install idl-files before going down to subdirs
$(foreach arch,$(TARGET_SYSTEMS), $(OBJ_DIR)/OBJ-$(arch)): $(addprefix $(INSTALLDIR_LOCAL)/,$(INSTALL_TARGET))

endif	# architecture is defined, really build
#####################################################
#
# Common part
#
#####################################################

-include $(DEPSVAR)
.PHONY: all clean cleanall config help install oldconfig txtconfig

help::
	@echo "  all            - generate .c and .h from idl files and install locally"
ifneq ($(SYSTEM),)
	@echo "                   to $(INSTALLDIR_LOCAL)"
endif
	@echo "  scrub          - delete backup and temporary files"
	@echo "  clean          - delete generated source files"
	@echo "  cleanall       - delete all generated, backup and temporary files"
	@echo "  help           - this help"
	@echo
	@echo "  idls are: $(IDL)"


endif	# _L4DIR_MK_IDL_MK undefined

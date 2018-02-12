# vi:ft=make
# This makefile contains the generator for our basic pattern rules.
# These generators allow easy instantiation of own rule sets.

# variables for instantiating make rules.
# for C++ compile (CXX) use $(CXX), $(INT_CXX_NAME), and $(CXXFLAGS)
BID_CXX_COMPILER = CXX
BID_CXX_COMPILER_INT_NAME = INT_CXX_NAME
# for C compile (C) use $(CC), $(INT_CPP_NAME), and $(CFLAGS)
BID_C_COMPILER = CC
BID_C_COMPILER_INT_NAME = INT_CPP_NAME
# for F compile (Fortran) use $(FC)
BID_F_COMPILER = FC
BID_F_COMPILER_INT_NAME = FC
# for AS compile (AS) use $(CC), $(INT_CPP_NAME), and $(ASFLAGS)
BID_AS_COMPILER = CC
BID_AS_COMPILER_INT_NAME = INT_CPP_NAME

define BID_MAKE_RULE_CREATE_SUBDIR
  $$(if $$(wildcard $$(dir $$@)),,$$(VERBOSE)$(MKDIR) $$(dir $$@))
endef

# Template for our make rules.
# arg1: the target or target pattern (e.g., %.o)
# arg2: the prerequisites (e.g., %.c)
# arg3: the compiler pattern, used for determining the compiler
#       variables and default flags (e.g., CXX). The rule uses
#       $($(BID_<arg3>_COMPILER)) as compiler,
#       $($(BID_<arg3>_COMPILER_INT_NAME)) for gendep, and
#       $(<arg3>FLAGS) as flags
# arg4: extra command line flags for the compile
define BID_MAKE_RULE_template_gendep
$(1): $(2) $(GENERAL_D_LOC)
	@$$(COMP_MESSAGE)
	$(call BID_MAKE_RULE_CREATE_SUBDIR)
	$$(VERBOSE)$$(call MAKEDEP,$$($(BID_$(3)_COMPILER_INT_NAME))) $$($(BID_$(3)_COMPILER)) -c $$(DEPEND_FLAG) $$(CPPFLAGS) $$($(3)FLAGS) $(4) $$< -o $$@
	$$(DEPEND_VERBOSE)$$(call DEPEND_EXTEND_FUNC, $$(*F).d, $$(dir $$@).$$(notdir $$@).d)
endef

# Template for our make rules (make rules that do not use gendep)
define BID_MAKE_RULE_template_nongendep
$(1): $(2) $(GENERAL_D_LOC)
	@$$(COMP_MESSAGE)
	$(call BID_MAKE_RULE_CREATE_SUBDIR)
	$$(VERBOSE)$$($(BID_$(3)_COMPILER)) -c $$(DEPFLAGS) $$(DEPFLAGS_$(3)) $$(CPPFLAGS) $$($(3)FLAGS) $(4) $$< -o $$@
endef

# which template rule do we really use
BID_MAKE_RULE_template = $(BID_MAKE_RULE_template_nongendep)

#
# Generate the default set of make rules.
# targets: .o, .s.o, .pr.o, and .pr.s.o
#
define BID_GENERATE_DEFAULT_MAKE_RULES
  $(call BID_MAKE_RULE_template,$(1).o,$(2),$(3),$(4) $$(call BID_mode_var,NOPICFLAGS))
  $(call BID_MAKE_RULE_template,$(1).s.o,$(2),$(3),$(4) $$(PICFLAGS))
  $(call BID_MAKE_RULE_template,$(1).pr.o,$(2),$(3),$(4) $$(call BID_mode_var,NOPICFLAGS) -DPROFILE -pg)
  $(call BID_MAKE_RULE_template,$(1).pr.s.o,$(2),$(3),$(4) $$(PICFLAGS) -DPROFILE -pg)
endef

#
# Generate the C++ set of make rules.
# prerequisites: %.<arg1>  (usually %.cc)
# targets: %.o, %.s.o, %.ne.o, %.s.ne.o, %.pr.o, and %.pr.s.o
#
define BID_GENERATE_CXX_MAKE_RULES
  $(call BID_GENERATE_DEFAULT_MAKE_RULES,%,%.$(1),CXX)
  $(call BID_MAKE_RULE_template,%.ne.o,%.$(1),CXX,$$(CXXFLAGS_NOEXC) $$(call BID_mode_var,NOPICFLAGS))
  $(call BID_MAKE_RULE_template,%.s.ne.o,%.$(1),CXX,$$(CXXFLAGS_NOEXC) $$(PICFLAGS))
endef

#
# Generate the C set of make rules.
# prerequisites: %.<arg1> (usually %.c)
# targets: %.o, %.s.o, %.pr.o, and %.pr.s.o
#
define BID_GENERATE_C_MAKE_RULES
  $(call BID_GENERATE_DEFAULT_MAKE_RULES,%,%.$(1),C)
endef

#
# Generate the F set of make rules.
# prerequisites: %.<arg1> (usually %.f90)
# targets: %.o, %.s.o, %.pr.o, and %.pr.s.o
#
define BID_GENERATE_F_MAKE_RULES
  $(call BID_GENERATE_DEFAULT_MAKE_RULES,%,%.$(1),F)
endef

#
# Generate the ASM set of make rules.
# prerequisites: %.<arg1> (usually %.S)
# targets: %.o, %.s.o, %.pr.o, and %.pr.s.o
#
define BID_GENERATE_ASM_MAKE_RULES
  $(call BID_GENERATE_DEFAULT_MAKE_RULES,%$(BID_S_INFIX),%.$(1),AS,$(if $(filter .$(1),$(BID_ASM_CPP_FILE_EXTENSIONS)),-x assembler-with-cpp,))
endef

#
# arg 1: target pattern
#     2: source pattern
#     3: compiler
#     4: compiler options
define BID_GENERATE_I_MAKE_RULE
$(1): $(2) FORCE
	@$$(COMP_MESSAGE)
	$(call BID_MAKE_RULE_CREATE_SUBDIR)
	$$(VERBOSE)$(3) -E -H -dD $$(CPPFLAGS) $(4) $$(call absfilename,$$<) -o $$@
	$$(VERBOSE)$$(if $$(DO_SHOW_RESULT_FILE),$$(PAGER) $$@)
endef

#
# arg 1: source pattern
#     2: compiler
#     3: compiler options
define BID_GENERATE_S_MAKE_RULE
%.S: $(1) FORCE
	@$$(COMP_MESSAGE)
	$(call BID_MAKE_RULE_CREATE_SUBDIR)
	$$(VERBOSE)$(2) -S $(3) $(CPPFLAGS) $$(call absfilename,$$<) -fverbose-asm -o $$@
endef

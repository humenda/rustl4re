ifneq ($(SYSTEM),)

camloptfeature = $(if $(shell $(CAMLOPT) -h 2>&1 | grep -w -- $(1)),$(1))
COMPFLAGS      = -g $(if $(VERBOSE),,-verbose) -warn-error A \
                 -nostdlib -output-obj \
                 $(call camloptfeature,-nodynlink)

# the link is done because ocamlopt creates *.cmi and *.cmx in the source dir!
%.o: $(SRC_DIR)/%.ml #libasmrun.a
	@echo -e "  ... Compiling Ocaml $@"
	$(VERBOSE)$(RM) $(notdir $<)
	$(VERBOSE)$(LN) -sf $(SRC_DIR)/$(notdir $<)
	$(VERBOSE)$(CAMLOPT) $(COMPFLAGS) -I $(OCAML_INCDIR) -o $@.tmp.o $(notdir $<)
	$(VERBOSE)$(LD) -m $(LD_EMULATION) -r -N -nostdlib -o $@ $@.tmp.o -L $(OCAML_LIBDIR) -locaml_asmrun

# some versions of ocamlopt (e.g. 3.10.2) want to have a libasmrun.a
# although we specify -nostdlib...
libasmrun.a: fake.o
	ar r $@ $^

fake.c:
	$(VERBOSE)echo > $@

endif


CAMLC          = ocamlc
CAMLOPT        = ocamlopt

OCAML          = $(PKGDIR)/../ocaml
OCAML_INCDIR   = $(OBJ_BASE)/pkg/ocaml/ocaml/build/stdlib/OBJ-$(SYSTEM)
OCAML_LIBDIR   = $(OBJ_BASE)/pkg/ocaml/ocaml/build/asmrun/OBJ-$(SYSTEM)

GEN_OCAML      = echo -e "  ... Compiling Ocaml $(notdir $<) to $@"

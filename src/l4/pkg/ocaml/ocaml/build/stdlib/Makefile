PKGDIR	?= ../..
L4DIR	?= $(PKGDIR)/../../..

include $(L4DIR)/mk/prog.mk
include $(PKGDIR)/mk/defs.mk

ifneq ($(SYSTEM),)

COMPFLAGS     = -g -warn-error A -nostdlib
SPECIALFLAGS  = $(PKGDIR)/contrib/stdlib/Compflags

# from contrib/byterun/Makefile.common: $(PUBLIC_INCLUDES)
PUBLIC_INCLUDES = alloc.h callback.h config.h custom.h fail.h intext.h \
                  memory.h misc.h mlvalues.h printexc.h signals.h \
                  compatibility.h gc.h major_gc.h minor_gc.h freelist.h

# from contrib/stdlib/Makefile.shared: $(OBJS)
FILES = pervasives array list char string sys hashtbl sort marshal obj \
        int32 int64 nativeint lexing parsing set map stack queue \
        camlinternalLazy lazy stream buffer printf format scanf arg \
        printexc gc digest random callback camlinternalOO oo \
        camlinternalMod genlex weak filename complex arrayLabels \
        listLabels stringLabels moreLabels stdLabels
OBJS := $(addsuffix .ml,$(FILES))

vpath %.ml $(PKGDIR)/contrib/stdlib
vpath %.mli $(PKGDIR)/contrib/stdlib

all:: $(PUBLIC_INCLUDES) $(OBJS:.ml=.cmi) stdlib.cmxa

stdlib.cmxa: $(OBJS:.ml=.cmx)
	$(CAMLOPT) -a -o stdlib.cmxa $(OBJS:.ml=.cmx)

# link public header files
%.h : $(PKGDIR)/contrib/byterun/%.h
	@echo -e '  ... Linking $(notdir $<) to $@'
	$(VERBOSE)ln -s $^ $@

%.cmi : %.mli
	@$(GEN_OCAML)
	$(VERBOSE)$(CAMLC) $(COMPFLAGS) `$(SPECIALFLAGS) $@` -c -o $@ $<

%.cmi : %.ml
	@$(GEN_OCAML)
	$(VERBOSE)$(CAMLC) $(COMPFLAGS) `$(SPECIALFLAGS) $@` -c -o $@ $<

%.cmx : %.ml
	@$(GEN_OCAML)
	$(VERBOSE)$(CAMLOPT) $(COMPFLAGS) `$(SPECIALFLAGS) $@` -c -o $@ $<

# dependencies
$(addsuffix .cmi,$(filter-out pervasives,$(FILES))): pervasives.cmi

$(addsuffix .cmx,$(filter-out pervasives,$(FILES))): pervasives.cmx

pervasives.cmx: pervasives.cmi
oo.cmx: oo.cmi
parsing.cmx: parsing.cmi
printf.cmx: printf.cmi
printexc.cmx: printf.cmi
arg.cmx: arg.cmi

oo.cmi: camlinternalOO.cmi
parsing.cmi: lexing.cmi
printf.cmi: buffer.cmi
arg.cmi: printf.cmi
format.cmi: buffer.cmi

DEL_ON_CLEAN = $(addsuffix .cmi,$(FILES)) $(addsuffix .cmx,$(FILES)) \
               $(PUBLIC_INCLUDES) stdlib.cmxa stdlib.a

endif

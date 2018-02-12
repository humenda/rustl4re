PRIVATE_INCDIR = $(OCAML)/build/config \
                 $(OCAML)/build/ARCH-$(ARCH)/config \
                 $(OCAML)/build/stdlib \
                 $(OCAML_INCDIR)

# ok, so 3.11.x seems to be ok but 3.10 makes trouble (just see the
# libasmrun.a thing in libext2/lib/src/Makefile ), and additionally there
# are linking problems with such a generated lib.... so disable anything but
# 3.11 versions...
OVERSION    := $(subst ., ,$(shell $(CAMLOPT) -version))
OVMAJOR     := $(word 1, $(OVERSION))
OVMIDDLE    := $(word 2, $(OVERSION))

# reset some vars

ifeq ($(filter-out 1 2,$(OVMAJOR)),)
TARGET =
else
  ifeq ($(filter-out 0 1 2 3 4 5 6 7 8 9 10,$(OVMIDDLE)),)
    TARGET =
  endif
endif

ifeq ($(TARGET),)
REQUIRES_LIBS=
endif

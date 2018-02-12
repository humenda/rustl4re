PKGDIR        ?= ../../..
L4DIR         ?= $(PKGDIR)/../../..

PC_FILENAME    = libpciids
TARGET         = libpciids.a libpciids.so
SRC_C          = names.c
PRIVATE_INCDIR = .

include $(L4DIR)/mk/lib.mk

ifeq ($(BID_COMPILER_TYPE),gcc)
CFLAGS += -fvisibility=internal
endif

names.o names.s.o: devlist.h classlist.h

devlist.h classlist.h: $(SRC_DIR)/pci.ids gen-devlist $(GENERIC_LOC_D)
	$(VERBOSE)echo -e "  ... Generating devlist.h/classlist.h"
	$(VERBOSE)./gen-devlist < $<

gen-devlist: gen-devlist.c $(GENERIC_LOC_D)
	@$(BUILD_MESSAGE)
	$(VERBOSE)$(HOST_CC) -o $@ $<

update:
	$(VERBOSE)wget -O - http://pciids.sourceforge.net/v2.2/pci.ids.bz2 \
	   | bzcat > pci.ids

clean::
	$(VERBOSE)$(RM) devlist.h classlist.h

.PHONY: update

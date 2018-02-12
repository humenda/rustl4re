# -*- Makefile -*-
#
# DROPS (Dresden Realtime OPerating System) Component
#
# Makefile-Template for directories containing only subdirs
#
# 05/2002 Jork Loeser <jork.loeser@inf.tu-dresden.de>

include $(L4DIR)/mk/Makeconf

ifeq ($(PKGDIR),.)
TARGET ?= $(patsubst %/Makefile,%,$(wildcard $(addsuffix /Makefile, \
	idl include src lib server examples doc)))
$(if $(wildcard include/Makefile), idl lib server examples: include)
$(if $(wildcard idl/Makefile), lib server examples: idl)
$(if $(wildcard lib/Makefile), server examples: lib)
else
TARGET ?= $(patsubst %/Makefile,%,$(wildcard $(addsuffix /Makefile, \
	idl src lib server examples doc)))
endif
SUBDIR_TARGET	:= $(if $(filter doc,$(MAKECMDGOALS)),$(TARGET),    \
			$(filter-out doc,$(TARGET)))

all::	$(SUBDIR_TARGET) $(SUBDIRS)
install::

lib: include
server: include

clean cleanall scrub::
	$(VERBOSE)set -e; $(foreach d,$(TARGET), test -f $d/broken || \
	    if [ -f $d/Makefile ] ; then PWD=$(PWD)/$d $(MAKE) -C $d $@ $(MKFLAGS) $(MKFLAGS_$(d)); fi; )

install oldconfig txtconfig relink::
	$(VERBOSE)set -e; $(foreach d,$(TARGET), test -f $d/broken -o -f $d/obsolete || \
	    if [ -f $d/Makefile ] ; then PWD=$(PWD)/$d $(MAKE) -C $d $@ $(MKFLAGS) $(MKFLAGS_$(d)); fi; )

# first the subdir-targets (this is were "all" will be build, e.g. in lib
# or server.
$(filter-out ptest,$(SUBDIR_TARGET)): %:
	$(VERBOSE)test -f $@/broken -o -f $@/obsolete ||		\
	    if [ -f $@/Makefile ] ; then PWD=$(PWD)/$@ $(MAKE) -C $@ $(MKFLAGS) ; fi
# Second, the rules for going down into sub-pkgs with "lib" and "server"
# targets. Going down into sub-pkgs.
	$(if $(SUBDIRS),$(if $(filter $@,idl include lib server examples doc),\
		$(VERBOSE)set -e; for s in $(SUBDIRS); do \
			PWD=$(PWD)/$$s $(MAKE) -C $$s $@ $(MKFLAGS); done ))

idl include lib server examples doc:

# the test target is something special:
TEST_DEPENDS ?= server

# check if the test directory exists, check if its broken or obsolete
# to be able to specify additional dependencies, we make it a :: target
ptest:: $(TEST_DEPENDS) 
	$(VERBOSE)test -f $@/broken -o -f $@/obsolete || \
	  if [ -f $@/Makefile ] ; then PWD=$(PWD)/$@ $(MAKE) -C $@ $(MKFLAGS) ; fi

install-symlinks:
	$(warning target install-symlinks is obsolete. Use 'include' instead (warning only))
	$(VERBOSE)$(MAKE) include

help::
	@echo "  all            - build subdirs: $(SUBDIR_TARGET)"
	$(if $(filter doc,$(TARGET)), \
	@echo "  doc            - build documentation")
	@echo "  scrub          - call scrub recursively"
	@echo "  clean          - call clean recursively"
	@echo "  cleanall       - call cleanall recursively"
	@echo "  install        - build subdirs, install recursively then"
	@echo "  oldconfig      - call oldconfig recursively"
	@echo "  txtconfig      - call txtconfig recursively"

.PHONY: $(TARGET) all clean cleanall help install oldconfig txtconfig

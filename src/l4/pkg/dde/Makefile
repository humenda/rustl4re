PKGDIR	?= .
L4DIR	?= $(PKGDIR)/../..

# the default is to build the listed directories, provided that they
# contain a Makefile. If you need to change this, uncomment the following
# line and adapt it.
# TARGET = idl include src lib server examples doc

TARGET = include ddekit linux26 fbsd examples doc

linux26: ddekit

#DROPSCONF 				= y
#DROPSCONF_CONFIG_MK		= $(OBJ_DIR)/Makeconf

include $(L4DIR)/mk/subdir.mk

.PHONY: include lib

ddekit: include
include: all_includes
lib: all_libs include

all_includes:
	$(VERBOSE)PWD=$(PWD)/include                 $(MAKE) -C include
#	$(VERBOSE)PWD=$(PWD)/linux26/include         $(MAKE) -C linux26/include
#	$(VERBOSE)PWD=$(PWD)/linux26/contrib/include $(MAKE) -C linux26/contrib/include

all_libs:
	$(VERBOSE)PWD=$(PWD)/ddekit/src-c++          $(MAKE) -C ddekit/src-c++
#	$(VERBOSE)PWD=$(PWD)/linux26/lib             $(MAKE) -C linux26/lib

PKGDIR	?= ../../../../..
L4DIR	?= $(PKGDIR)/../..

# Force these include files to appear in a special subfolder of dde/
INSTALL_INC_PREFIX = l4/dde/linux26/linux-headers/

include $(L4DIR)/mk/include.mk

#MAKELINK = if [ ! $1 -ef $2 ] ; then ln -$(if $(VERBOSE),v)sf $2 $1 ; fi
#ARCH = $(BUILD_ARCH)
#
#all::
#	$(VERBOSE)$(MKDIR) $(INSTALLDIR_LOCAL)/$(ARCH)/$(INSTALL_INC_PREFIX)

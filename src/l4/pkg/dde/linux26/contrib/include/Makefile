PKGDIR	?= ../../..
L4DIR	?= $(PKGDIR)/../..

# Force these include files to appear in a special subfolder of dde/
INSTALL_INC_PREFIX = l4/dde/linux26/linux-headers

include $(L4DIR)/mk/include.mk

#SUFFIX_amd64 = x86_64
#SUFFIX_amd64 = i386 #hack!
#SUFFIX_x86   = x86 
#SUFFIX_arm   = arm
#
#SUFFIX_rv = realview
#SUFFIX_int = integrator

# macro to establish a symlink to a dir if not already existing
#MAKELINK = if [ ! $1 -ef $2 ] ; then ln -$(if $(VERBOSE),v)sf $2 $1 ; fi
#
#ARCH = $(BUILD_ARCH)
#PLAT = rv
#
#all::
#	$(VERBOSE)$(MKDIR) $(INSTALLDIR_LOCAL)/$(ARCH)/$(INSTALL_INC_PREFIX)
#	$(VERBOSE)$(call MAKELINK, \
#		$(INSTALLDIR_LOCAL)/$(ARCH)/$(INSTALL_INC_PREFIX)/asm,           \
#		$(INSTALLDIR_LOCAL)/$(INSTALL_INC_PREFIX)/asm-$(SUFFIX_$(ARCH)))
#ifeq ($(BUILD_ARCH),arm)
#	$(VERBOSE)$(call MAKELINK, \
#		$(INSTALLDIR_LOCAL)/$(ARCH)/$(INSTALL_INC_PREFIX)/asm/arch,      \
#		$(INSTALLDIR_LOCAL)/$(INSTALL_INC_PREFIX)/asm-$(SUFFIX_$(ARCH))/arch-$(SUFFIX_$(PLAT)))
#endif
#
#install::
#	$(VERBOSE)$(MKDIR) $(INSTALLDIR)/$(ARCH)/$(INSTALL_INC_PREFIX)
#	$(VERBOSE)$(call MAKELINK, \
#		$(INSTALLDIR)/$(ARCH)/$(INSTALL_INC_PREFIX)/asm                    \
#		$(INSTALLDIR)/$(ARCH)/$(INSTALL_INC_PREFIX)/asm-$(SUFFIX_$(ARCH)))


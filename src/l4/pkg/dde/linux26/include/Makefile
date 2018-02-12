PKGDIR	?= ../..
L4DIR	?= $(PKGDIR)/../..

# Force these include files to appear in a special subfolder of dde/
INSTALL_INC_PREFIX = l4/dde/linux26

include $(L4DIR)/mk/Makeconf
-include $(PKGDIR_OBJ)/Makeconf

include $(L4DIR)/mk/include.mk

#ifeq ($(CONFIG_DDE26),y)
ifneq ($(BUILD_ARCH),amd64)
ifneq ($(BUILD_ARCH),ppc32)
ifneq ($(BUILD_ARCH),sparc)
ifneq ($(BUILD_ARCH),arm64)
ifneq ($(BUILD_ARCH),mips)
SUFFIX_amd64 = x86_64
SUFFIX_x86   = x86
SUFFIX_arm   = arm
SUFFIX_arm64 = arm64
SUFFIX_ppc32 = powerpc
SUFFIX_sparc = sparc
SUFFIX_mips  = mips

# macro to establish a symlink to a dir if not already existing
MAKELINK = if [ ! $1 -ef $2 ] ; then ln -sf $3 $2 ; fi

ARCH = $(BUILD_ARCH)

all::
	$(VERBOSE)$(call MAKELINK, \
		$(INSTALLDIR_LOCAL)/$(ARCH)/$(INSTALL_INC_PREFIX)/asm-$(SUFFIX_$(ARCH)),\
		$(INSTALLDIR_LOCAL)/$(ARCH)/$(INSTALL_INC_PREFIX)/asm, asm-$(SUFFIX_$(ARCH)))

install::
	$(VERBOSE)$(call MAKELINK, \
		$(INSTALLDIR)/$(ARCH)/$(INSTALL_INC_PREFIX)/asm-$(SUFFIX_$(ARCH)),\
		$(INSTALLDIR)/$(ARCH)/$(INSTALL_INC_PREFIX)/asm, asm-$(SUFFIX_$(ARCH)))
endif
endif
endif
endif
endif
#endif

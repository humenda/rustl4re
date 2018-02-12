PKGDIR ?= ../..
L4DIR  ?= $(PKGDIR)/../..

include $(L4DIR)/mk/Makeconf
-include $(PKGDIR_OBJ)/Makeconf

ifeq ($(CONFIG_FBSD),y)
INSTALL_INC_PREFIX = l4/dde/fbsd

include $(L4DIR)/mk/include.mk
endif

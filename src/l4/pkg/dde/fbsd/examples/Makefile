PKGDIR ?= ../..
L4DIR  ?= $(PKGDIR)/../..

include $(L4DIR)/mk/Makeconf
-include $(PKGDIR_OBJ)/Makeconf

ifeq ($(CONFIG_FBSD),y)
TARGET  = l4ata tests
endif

include $(L4DIR)/mk/subdir.mk

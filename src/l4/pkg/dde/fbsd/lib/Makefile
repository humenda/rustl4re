PKGDIR ?= ../..
L4DIR  ?= $(PKGDIR)/../..

include $(L4DIR)/mk/Makeconf
-include $(PKGDIR_OBJ)/Makeconf

ifeq ($(CONFIG_FBSD),y)
TARGET  = common ata
endif

include $(L4DIR)/mk/subdir.mk

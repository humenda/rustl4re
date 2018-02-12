# generic plugin stuff

TARGET           := libmag-$(PLUGIN).a
ifneq ($(PLUGIN_STATIC_ONLY),y)
TARGET           += libmag-$(PLUGIN).so
endif
LDFLAGS          += --unique
LINK_INCR        := libmag-$(PLUGIN).a
LDFLAGS_libmag-$(PLUGIN).so          += -lmag-plugin.o

PC_FILENAME      := mag-$(PLUGIN)
REQUIRES_LIBS    += libstdc++

include $(L4DIR)/mk/lib.mk
$(GENERAL_D_LOC): $(PKGDIR)/plugins/plugin.mk

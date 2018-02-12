PKGDIR           ?= ../..
L4DIR            ?= $(PKGDIR)/../..
DOPEDIR          ?= $(L4DIR_ABS)/pkg/dope

TARGET            = mag
PRIVATE_INCDIR    = $(SRC_DIR)/../../include/server
SRC_CC           := big_mouse.cc main.cc screen.cc view_stack.cc \
                    user_state.cc plugin.cc input_driver.cc object_gc.cc \
                    input_source.cc session.cc core_api.cc lua_glue.swg.cc
SRC_DATA         := mag.lua default.tff

#MODE		 := shared

STATIC_PLUGINS += mag-input-libinput
STATIC_PLUGINS += mag-input-event
STATIC_PLUGINS += mag-client_fb
STATIC_PLUGINS += mag-mag_client

REQUIRES_LIBS:= libsupc++ libdl mag-gfx lua++ cxx_libc_io cxx_io
REQUIRES_LIBS += $(STATIC_PLUGINS)
#LDFLAGS += --export-dynamic

CPPFLAGS += -fvisibility=hidden

CXXFLAGS_lua_glue.swg.cc += -Wno-misleading-indentation

include $(L4DIR)/mk/prog.mk

# regenerate shipped files
ifneq ($(REGEN_FILES),)
all:: $(SRC_DIR)/lua_glue.swg.h $(SRC_DIR)/lua_glue.swg.cc

SWIG_RM_EXT_C ?= $(L4DIR)/tool/bin/swig-rm-extern-c.pl
SWIG          ?= swig

%.swg:

$(SRC_DIR)/%.swg.cc: $(SRC_DIR)/%.swg.c_cc $(SRC_DIR)/Makefile
	@$(GEN_MESSAGE)
	$(VERBOSE)$(SWIG_RM_EXT_C) $< >$@

$(SRC_DIR)/%.swg.h: $(SRC_DIR)/%.swg.c_h
	@$(GEN_MESSAGE)
	$(VERBOSE)$(SWIG_RM_EXT_C) $< >$@

SWIG_INCDIR:=-I$(SRC_DIR) -I$(SRC_DIR)/../../include -I$(L4DIR)/pkg

$(SRC_DIR)/%.swg.c_cc: $(SRC_DIR)/%.swg $(SRC_DIR)/Makefile
	@$(GEN_MESSAGE)
	$(VERBOSE)$(SWIG) $(SWIG_INCDIR) -c++ -small -lua -o $@ $<

$(SRC_DIR)/%.swg.c_h:
	@$(GEN_MESSAGE)
	$(VERBOSE)$(SWIG) $(SWIG_INCDIR) -c++ -small -lua -external-runtime $@

endif

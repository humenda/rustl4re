PKGDIR    ?= ..
L4DIR     ?= $(PKGDIR)/../..

include $(L4DIR)/mk/Makeconf

CONTRIB_HEADERS  = y
CINCPATH        := $(call absfilename,$(SRC_DIR)/../contrib/include)
MOD_FILES       := SDL_config.h SDL_config_l4re.h
TARGET           = $(MOD_FILES) \
                   $(filter-out $(addprefix $(CINCPATH)/,$(MOD_FILES)),$(wildcard $(CINCPATH)/*.h))

include $(L4DIR)/mk/include.mk

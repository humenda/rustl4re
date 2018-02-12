PKGDIR          ?= ../..
L4DIR           ?= $(PKGDIR)/../..

TARGET          = libquadmath.a libquadmath.so
PC_FILENAMES	= libquadmath
SYSTEMS		= x86-l4f amd64-l4f

SRC_C += $(patsubst $(CONTRIB_DIR)/%,%,$(wildcard $(CONTRIB_DIR)/math/*.c))
SRC_C += $(patsubst $(CONTRIB_DIR)/%,%,$(wildcard $(CONTRIB_DIR)/printf/*.c))

include $(L4DIR)/mk/Makeconf

CONTRIB_DIR     = $(SRC_DIR)/../contrib
PRIVATE_INCDIR  = $(SRC_DIR) $(CONTRIB_DIR)
WARNINGS        = $(WARNINGS_MINIMAL) \
                  -Wno-unused-variable -Wno-unused-but-set-variable \
                  -Wno-misleading-indentation

vpath %.c   $(CONTRIB_DIR)

include $(L4DIR)/mk/lib.mk

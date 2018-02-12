PKGDIR          ?= ../..
L4DIR           ?= $(PKGDIR)/../..

TARGET          = libl4png.a libl4png.so
REQUIRES_LIBS   = zlib
CONTRIB_INCDIR  = libpng

SRC_C-arm       = arm/arm_init.c arm/filter_neon_intrinsics.c
SRC_S-arm       = arm/filter_neon.S
SRC_C-arm64     = $(SRC_C-arm)
SRC_C-mips      = mips/mips_init.c mips/filter_msa_intrinsics.c
SRC_C-x86       = intel/intel_init.c intel/filter_sse2_intrinsics.c
SRC_C-amd64     = $(SRC_C-x86)
SRC_C           = pngerror.c pngget.c pngpread.c pngrio.c pngrutil.c \
                  pngwrite.c pngwutil.c png.c pngmem.c pngread.c \
                  pngrtran.c pngset.c pngtrans.c pngwio.c pngwtran.c \
                  $(SRC_C-$(ARCH))
SRC_S           = $(SRC_S-$(ARCH))

PRIVATE_INCDIR  = $(SRC_DIR)

vpath %.c $(SRC_DIR)/../contrib
vpath %.S $(SRC_DIR)/../contrib

include $(L4DIR)/mk/lib.mk

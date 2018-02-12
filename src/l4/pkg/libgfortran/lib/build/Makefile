PKGDIR          ?= ../..
L4DIR           ?= $(PKGDIR)/../..

TARGET          = libgfortran.a libgfortran.so
PC_FILENAMES	= libgfortran

# The -fallow-leading-underscore is only needed for some Fortran
# files, but it does not hurt for all the others.
CFLAGS         += -fcx-fortran-rules
FFLAGS         += -fno-underscoring -fimplicit-none -fno-repack-arrays \
                  -fallow-leading-underscore

include $(L4DIR)/mk/Makeconf

REQUIRES_LIBS = libc_be_sig libc_support_misc libstdc++ libc \
                libquadmath libbacktrace

include $(SRC_DIR)/Makefile.dist-c
include $(SRC_DIR)/Makefile.dist-f

i_matmul_c  = $(filter generated/matmul_i%,$(C_FILES)) \
	      $(filter generated/matmul_r%,$(C_FILES)) \
	      $(filter generated/matmul_c%,$(C_FILES))
i_matmull_c = $(filter generated/matmul_l%,$(C_FILES))

$(foreach f,$(eval CFLAGS_$(f) = -ftree-vectorize -funroll-loops),$(i_matmul_c))
$(foreach f,$(eval CFLAGS_$(f) = -funroll-loops),$(i_matmull_c))

IEEE_FFLAGS = -Wno-unused-dummy-argument -Wno-c-binding-type \
              -ffree-line-length-0 -fallow-leading-underscore

$(GENERAL_D_LOC): nonpic pic

nonpic pic:
	$(VERBOSE)$(MKDIR) $@

FFLAGS_ieee/ieee_arithmetic.o   = -Jnonpic $(IEEE_FFLAGS)
FFLAGS_ieee/ieee_exceptions.o   = -Jnonpic $(IEEE_FFLAGS)
FFLAGS_ieee/ieee_features.o     = -Jnonpic $(IEEE_FFLAGS)

FFLAGS_ieee/ieee_arithmetic.s.o = -Jpic $(IEEE_FFLAGS)
FFLAGS_ieee/ieee_exceptions.s.o = -Jpic $(IEEE_FFLAGS)
FFLAGS_ieee/ieee_features.s.o   = -Jpic $(IEEE_FFLAGS)

SRC_C_libgfortran.a      = $(C_FILES)
SRC_F_libgfortran.a      = $(F_FILES)
SRC_C_libgfortran.so     = $(C_FILES)
SRC_F_libgfortran.so     = $(F_FILES)

$(SRC_DIR)/Makefile: $(SRC_DIR)/Makefile.dist-c $(SRC_DIR)/Makefile.dist-f

CONTRIB_DIR     = $(SRC_DIR)/../contrib
PRIVATE_INCDIR  = $(SRC_DIR) $(SRC_DIR)/include $(CONTRIB_DIR)
PRIVATE_INCDIR += $(SRC_DIR)/ARCH-$(ARCH)
PRIVATE_INCDIR += $(OBJ_BASE)/include/contrib/libstdc++-v3/bits

vpath %.c   $(CONTRIB_DIR)
vpath %.f90 $(CONTRIB_DIR)
vpath %.F90 $(CONTRIB_DIR)

include $(L4DIR)/mk/lib.mk

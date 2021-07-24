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

$(foreach f,$(i_matmul_c),$(eval CFLAGS_$(f) = -ftree-vectorize -funroll-loops))
$(foreach f,$(i_matmull_c),$(eval CFLAGS_$(f) = -funroll-loops))

IEEE_FFLAGS = -Wno-unused-dummy-argument -Wno-c-binding-type \
              -ffree-line-length-0 -fallow-leading-underscore

IEEE_FILES = $(addprefix ieee/,ieee_arithmetic.F90 ieee_exceptions.F90 \
                               ieee_features.F90)

PICFLAGS   += -Jpic
NOPICFLAGS += -Jnonpic

$(IEEE_FILES): pic/. nonpic/.

pic/. nonpic/.:
	$(VERBOSE)$(call create_dir,$(@D))

$(foreach f,$(IEEE_FILES),$(eval FFLAGS_$(f) = $(IEEE_FFLAGS)))

SRC_C += $(C_FILES)
SRC_F += $(F_FILES)

$(SRC_DIR)/Makefile: $(SRC_DIR)/Makefile.dist-c $(SRC_DIR)/Makefile.dist-f

CONTRIB_DIR     = $(SRC_DIR)/../contrib
PRIVATE_INCDIR  = $(SRC_DIR) $(SRC_DIR)/include $(CONTRIB_DIR)
PRIVATE_INCDIR += $(SRC_DIR)/ARCH-$(ARCH)
PRIVATE_INCDIR += $(OBJ_BASE)/include/contrib/libstdc++-v3/bits

vpath %.c   $(CONTRIB_DIR)
vpath %.f90 $(CONTRIB_DIR)
vpath %.F90 $(CONTRIB_DIR)

include $(L4DIR)/mk/lib.mk

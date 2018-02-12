PKGDIR          ?= ../..
L4DIR           ?= $(PKGDIR)/../..

DEPENDS_PKGS     = libgomp
# missing sync_* functions in (my) sparc compiler
SYSTEMS          = $(filter-out sparc-l4f,$(SYSTEMS_ABI))

include $(L4DIR)/mk/Makeconf

ifneq ($(SYSTEM),)
  # check whether used gcc has the omp.h file available
  OMP_H_PATH := $(shell $(CC) -print-file-name=include/omp.h)

  ifeq ($(strip $(OMP_H_PATH)),include/omp.h)
    $(info [32m $(CC) does not have omp.h header file available, skipping.[0m)
  else
    TARGET        = $(if $(filter 4.2 4.3 4.4 4.5 4.6 4.7 4.8 4.9 5,$(GCCVERSION)),ex_omp)
    REQUIRES_LIBS = libgomp libc_support_misc
  endif
endif

SRC_C            = main.c
CFLAGS           = -fopenmp

include $(L4DIR)/mk/prog.mk

PKGDIR         := ..
L4DIR          ?= $(PKGDIR)/../..

TARGET          = python
MODE            = shared
REQUIRES_LIBS   = libpython libc_be_file_stdin \
                  libc_be_sig libc_be_math readline cxx_libc_io cxx_io
PRIVATE_INCDIR  = $(PKGDIR_ABS)/include $(PKGDIR_ABS)/include/ARCH-$(ARCH)
SRC_CC          = python.cc 
PRIVATE_INCDIR += $(PKGDIR_ABS)/contrib/Include
LDFLAGS        += --export-dynamic

include $(L4DIR)/mk/prog.mk

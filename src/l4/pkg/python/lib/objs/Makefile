PKGDIR  ?= ../..
L4DIR   ?= $(PKGDIR)/../..

TARGET = python-mod/_functools.so 
SRC_C_python-mod/_functools.so  = Modules/_functoolsmodule.c
TARGET += python-mod/_collections.so
SRC_C_python-mod/_collections.so  = Modules/_collectionsmodule.c
TARGET += python-mod/operator.so
SRC_C_python-mod/operator.so = Modules/operator.c


PC_FILENAME :=

CXXFLAGS := -fno-strict-aliasing
CCFLAGS  := -fno-strict-aliasing

PRIVATE_INCDIR = $(PKGDIR_ABS)/include $(PKGDIR_ABS)/include/ARCH-$(ARCH)
PRIVATE_INCDIR += $(PKGDIR_ABS)/contrib/Include
PRIVATE_INCDIR += $(OBJ_BASE)/include/l4/python

vpath %.c $(PKGDIR)/contrib

include $(L4DIR)/mk/lib.mk

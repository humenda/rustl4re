PKGDIR		?= ..
L4DIR		?= $(PKGDIR)/../..

vpath %.cc $(PKGDIR)/src/ARCH-$(ARCH)

TARGET		= libcxx_thread
SRC_CC		= thread.cc thread-arch.cc
PC_FILENAME     = cxx_thread

CXX_PKG_DIR=$(L4DIR)/pkg/l4re-core/cxx
include $(CXX_PKG_DIR)/lib/Makefile.inc
include $(L4DIR)/mk/lib.mk

$(GENERAL_D_LOC): $(CXX_PKG_DIR)/lib/Makefile.inc

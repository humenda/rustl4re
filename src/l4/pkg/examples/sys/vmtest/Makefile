PKGDIR        ?= ../..
L4DIR         ?= $(PKGDIR)/../..

TARGET        = ex_vmtest
SYSTEMS       = x86-l4f amd64-l4f
SRC_S         = guest.S
SRC_CC        = vm.cc vmx.cc svm.cc main.cc
REQUIRES_LIBS = libvcpu l4util

include $(L4DIR)/mk/prog.mk

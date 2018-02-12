PKGDIR		?= ../..
L4DIR		?= $(PKGDIR)/../..

TARGET		= libcunit.a
PC_FILENAME     = libcunit
REQUIRES_LIBS   = log
CONTRIB_INCDIR  = cunit

vpath %.c : $(PKGDIR)/lib/common/Framework
vpath Basic.c : $(PKGDIR)/lib/common/Basic

SRC_C		= CUError.c MyMem.c TestDB.c TestRun.c Util.c Basic.c

include $(L4DIR)/mk/lib.mk

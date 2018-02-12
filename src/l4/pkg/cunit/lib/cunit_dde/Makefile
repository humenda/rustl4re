PKGDIR		?= ../..
L4DIR		?= $(PKGDIR)/../..

TARGET		= libcunit_dde.a
SYSTEMS		= x86-l4f
PC_FILENAME     = libcunit_dde
REQUIRES_LIBS   = log

include $(PKGDIR)/../dde/linux26/Makeconf

vpath %.c : $(PKGDIR)/lib/common/Framework
vpath Basic.c : $(PKGDIR)/lib/common/Basic

PRIVATE_INCDIR += $(PKGDIR)/lib/cunit_dde

SRC_C		= CUError.c MyMem.c TestDB.c TestRun.c Util.c Basic.c

CFLAGS += -DDDE_LINUX

include $(L4DIR)/mk/lib.mk

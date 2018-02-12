PKGDIR  ?= ../..
L4DIR   ?= $(PKGDIR)/../..

# a simpler build would be to avoid -DTLSF_USE_LOCKS=1 so that no pthreads
# are used

TARGET           = libtlfs.a libtlfs.so
PC_FILENAME      = libtlsf
CONTRIB_INCDIR   = libtlsf
REQUIRES_LIBS    = libpthread
SRC_C           += tlsf.c
DEFINES          = -DTLSF_USE_LOCKS=1
DEFINES         += -DUSE_MMAP=1
DEFINES         += -DTLSF_STATISTIC

vpath %.c $(PKGDIR)/lib/contrib/src

include $(L4DIR)/mk/lib.mk

CFLAGS          += -Wno-unused

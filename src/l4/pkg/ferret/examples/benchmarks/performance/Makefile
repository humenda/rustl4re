PKGDIR         ?= ../../..
L4DIR          ?= $(PKGDIR)/../..

SYSTEMS         = x86-l4f

SRC_C_fer_perf_test   = perf.c
#SRC_C_fer_thread_test = threads.c

TARGET          = fer_perf_test #fer_thread_test

REQUIRES_LIBS+= ferret-producer ferret-common libstdc++

OPTS = -O3 -fno-strict-aliasing $(CARCHFLAGS_$(ARCH)) $(CARCHFLAGS_$(ARCH)_$(CPU)) -fomit-frame-pointer


include $(L4DIR)/mk/prog.mk

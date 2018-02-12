PKGDIR     ?= ../../..
L4DIR      ?= $(PKGDIR)/../..

TARGET      = libpthread_rep.a #libpthread_rep.so

PC_FILENAME = libpthread_rep
PC_LIBS_libpthread_rep := -lpthread_rep
PC_EXTRA := Link_Libs= %{static:-lpthread_rep}

SYSTEMS = x86-l4f

CONTRIBDIR = $(PKGDIR)/../l4re-core/uclibc/lib/contrib/uclibc
ORIGDIR    = $(PKGDIR)/../l4re-core/uclibc/lib/libpthread/src
LDSODIR    = $(CONTRIBDIR)/ldso/ldso

SRC_CC      = manager.cc l4.cc
SRC_C      += spinlock.c mutex.c condvar.c rwlock.c errno.c specific.c \
              semaphore.c attr.c barrier.c join.c pthread.c \
              cancel.c ptcleanup.c errno-loc.c signals.c

ifeq ($(ARCH), x86)
SRC_C += pspinlock-i386.c
endif

SRC_C_libpthread_rep.a += libc_pthread_init.c
SRC_S       = tramp-$(ARCH).S
SO_INIT     = so_init-$(ARCH)
WARNINGS    = $(WARNINGS_MEDIUM)

vpath %.c  $(ORIGDIR) $(ORIGDIR)/sysdeps/generic
vpath %.cc $(ORIGDIR) $(ORIGDIR)/sysdeps/generic
vpath %.S  $(ORIGDIR) $(ORIGDIR)/sysdeps/generic
#vpath %.c $(SRC_DIR)/sysdeps/generic
#vpath dl-tls.c $(PKGDIR)/lib/contrib/uclibc/ldso/ldso

#DIR_amd64 := x86_64
DIR_x86   := i386
#DIR_arm   := arm
#DIR_ppc32 := powerpc
#DIR_sparc := sparc

INCDIR-x86   = $(ORIGDIR)/sysdeps/i386/i686 $(ORIGDIR)/sysdeps/i386 \
               $(CONTRIBDIR)/libc \
               $(LDSODIR)/$(DIR_$(ARCH))
#INCDIR-amd64 = $(ORIGDIR)/sysdeps/$(DIR_$(ARCH))
#INCDIR-arm   = $(ORIGDIR)/sysdeps/arm
#INCDIR-ppc32 = $(ORIGDIR)/sysdeps/powerpc
#INCDIR-sparc = $(ORIGDIR)/sysdeps/sparc

PRIVATE_INCDIR = -I- $(SRC_DIR) $(ORIGDIR) $(INCDIR-$(ARCH)) $(CONTRIBDIR)/ldso/include \
				 $(SRC_DIR)/../include

DEFINES   := -DL4_THREAD_SAFE -D__USE_XOPEN2K -D_POSIX_C_SOURCE=200112L \
             -D_GNU_SOURCE=1 -D__NO_WEAK_PTHREAD_ALIASES=1 \
	         -D_LIBC=1 -DIS_IN_libpthread=1 -DNOT_IN_libc=1 #-DPT_SOLO

DEFINES_x86-l4f   := -DUSE___THREAD=1 
#DEFINES_amd64-l4f := -DUSE___THREAD=1 
#DEFINES_arm-l4f   := -DUSE___THREAD=1 
#DEFINES_ppc32-l4f := -DUSE___THREAD=1 
#DEFINES_sparc-l4f := -DUSE___THREAD=1 

CFLAGS    += -include $(CONTRIBDIR)/include/libc-symbols.h -include $(SRC_DIR)/descr.h
CXXFLAGS  += -include $(CONTRIBDIR)/include/libc-symbols.h -include $(SRC_DIR)/descr.h
CXXFLAGS  += -fno-exceptions

include $(L4DIR)/mk/lib.mk
#CRT0 = $(CRTI_so) $(SO_INIT).s.o $(CRTBEGIN_so) $(CRT1_so)

PICFLAGS += -DSHARED

#libpthread.so: $(SO_INIT).s.o

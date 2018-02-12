PKGDIR     ?= ../../../..
L4DIR      ?= $(PKGDIR)/../..

TARGET      = lib$(PKGNAME)-bsd.a
SYSTEMS     = x86-l4f

SRC_C  = $(addprefix contrib/libkern/, ashldi3.c ashrdi3.c divdi3.c \
           lshrdi3.c moddi3.c qdivrem.c ucmpdi2.c udivdi3.c umoddi3.c \
           index.c rindex.c strcat.c strcmp.c strcpy.c strdup.c strlcat.c \
           strlcpy.c strlen.c strncmp.c strncpy.c strsep.c strtol.c \
           strtoq.c strtoul.c strtouq.c strvalid.c \
           bcd.c crc32.c random.c )
SRC_C += $(addprefix contrib/kern/, subr_eventhandler.c \
           subr_hints.c subr_kobj.c kern_sema.c subr_scanf.c kern_module.c \
           kern_mtxpool.c kern_environment.c subr_taskqueue.c \
           subr_module.c subr_rman.c subr_autoconf.c kern_intr.c kern_sx.c )
SRC_C +=   contrib/dev/pci/pci.c contrib/i386/i386/busdma_machdep.c
SRC_C += $(addprefix modified/kern/, subr_bus.c subr_prf.c subr_sbuf.c \
           kern_mutex.c kern_condvar.c subr_witness.c kern_tc.c \
           kern_synch.c subr_sleepqueue.c kern_kthread.c \
           kern_malloc.c kern_mib.c )
SRC_C +=   modified/vm/uma_dbg.c
GENERATED_C = $(addprefix generated/, bus_if.c device_if.c \
           isa_if.c pci_if.c pcib_if.c vnode_if.c majors.c acpi_if.c )
SRC_C += $(GENERATED_C) 
SRC_C += $(addprefix dummies/, \
		   tty.c harvest.c various.c sysctl.c devfs.c )
SRC_C += $(addprefix src/, \
	   panic.c init.c sysinit.c module.c copyin.c bzero.c pmap.c \
	   uma.c contigmalloc.c thread.c sched.c cold.c kthread.c pcib.c \
	   autoconf.c interrupt.c nexus.c )
SRC_C +=   src/timeout_self.c
#SRC_C +=   contrib/kern/kern_timeout.c src/timeout_bsd.c

vpath %.c $(OBJ_DIR)

CFLAGS_acpi_if.c = -I$(PKGDIR_ABS)/fbsd/lib/include/bsd/contrib/contrib/dev/acpica/
CFLAGS_kthread.c = -w

# use definitions in Makefile.inc in BSD mode
DDE_FBSD_ENV = BSD
include $(PKGDIR)/fbsd/Makefile.inc
include $(L4DIR)/mk/Makeconf
PRIVATE_INCDIR += $(DDE_FBSD_INCDIR)
DEFINES        += $(DDE_FBSD_DEFINES) -D_STDARG_H
ASFLAGS        += $(DDE_FBSD_ASFLAGS)
CFLAGS         += $(DDE_FBSD_CFLAGS)

all:: $(addprefix $(OBJ_DIR)/, $(GENERATED_C)) $(PKGDIR_OBJ)/fbsd/lib/include/bsd/contrib/machine

$(PKGDIR_OBJ)/fbsd/lib/include/bsd/contrib/machine:
	mkdir -p $(PKGDIR_OBJ)/fbsd/lib/include/bsd/contrib \
	         $(PKGDIR_OBJ)/fbsd/lib/include/bsd/modified
	ln -sf $(PKGDIR)/fbsd/lib/include/bsd/contrib/i386/include \
	         $(PKGDIR_OBJ)/fbsd/lib/include/bsd/contrib/machine
	ln -sf $(PKGDIR)/fbsd/lib/include/bsd/modified/i386/include \
	         $(PKGDIR_OBJ)/fbsd/lib/include/bsd/modified/machine

$(addprefix $(OBJ_DIR)/, $(GENERATED_C)): $(PKGDIR_ABS)/fbsd/lib/generate.sh
	$(VERBOSE) $(PKGDIR_ABS)/fbsd/lib/generate.sh $(PKGDIR_ABS)/fbsd/lib $(PKGDIR_OBJ)/fbsd/lib
foo:
	echo $(PRIVATE_INCDIR)

cleanall::
	$(VERBOSE) rm -f $(PKGDIR_OBJ)/fbsd/lib/common/bsd/generated/*
	$(VERBOSE) rm -f $(PKGDIR_OBJ)/fbsd/lib/include/bsd/generated/*
	$(VERBOSE) rm -f $(PKGDIR_OBJ)/fbsd/lib/include/bsd/contrib/machine
	$(VERBOSE) rm -f $(PKGDIR_OBJ)/fbsd/lib/include/bsd/modified/machine

include $(L4DIR)/mk/lib.mk

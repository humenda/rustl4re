PKGDIR     ?= ../../..
L4DIR      ?= $(PKGDIR)/../..

TARGET      = lib$(PKGNAME)-ata.a
SYSTEMS     = x86-l4f

DISK_WITH_GEOM = no

SRC_C      += $(addprefix src/,                                           \
				dad_disk.c dad_geom.c dad_request.c bio.c various.c )
SRC_C      += $(addprefix contrib/kern/,                                  \
				subr_devstat.c subr_disk.c )
SRC_C      += $(addprefix contrib/geom/,                                  \
				geom_dump.c geom_subr.c geom_event.c geom_kern.c )
SRC_C      += $(addprefix modified/geom/,                                 \
				geom_io.c geom_ctl.c )
SRC_C      += $(addprefix contrib/dev/ata/,                               \
				ata-all.c ata-cbus.c ata-chipset.c ata-disk.c ata-dma.c   \
				ata-isa.c ata-lowlevel.c ata-pci.c ata-raid.c ata-queue.c \
				atapi-cd.c atapi-fd.c )

ifeq (${DISK_WITH_GEOM}, yes)
SRC_C      += $(addprefix contrib/geom/, geom_disk.c )
else
SRC_C      += $(addprefix src/, nogeom-disk.c )
endif

CFLAGS_ata-lowlevel.c = -w
CFLAGS_ata-raid.c     = -w
CFLAGS_ata-queue.c    = -w

PRIVATE_INCDIR += $(addprefix $(PKGDIR)/fbsd/lib/ata/, src contrib )


DDE_FBSD_ENV    = BSD
include $(PKGDIR)/fbsd/Makefile.inc
PRIVATE_INCDIR += $(DDE_FBSD_INCDIR)
DEFINES        += $(DDE_FBSD_DEFINES) -D_STDARG_H
ASFLAGS        += $(DDE_FBSD_ASFLAGS)
CFLAGS         += $(DDE_FBSD_CFLAGS)
LIBS           += $(DDE_FBSD_LIBS)


include $(L4DIR)/mk/lib.mk

PKGDIR		?= ../../..
L4DIR		?= $(PKGDIR)/../..

TARGET          = libcon_hw.a
REQUIRES_LIBS   = libio libio-vbus linux-26-headers
NOTARGETSTOINSTALL = 1

OPTS		= -g -Os $(CARCHFLAGS_$(ARCH)) $(CARCHFLAGS_$(ARCH)_$(CPU))
SRC_C_amd64_libcon_hw.a = ati.c ati_vid.c \
		  ati128.c ati128_vid.c \
		  radeon.c \
		  matrox.c matrox_vid.c \
		  savage.c savage_vid.c \
		  vmware.c \
		  riva.c \
		  intel.c \
		  init.c pci.c iomem.c \
		  iomem.c
SRC_C_x86_libcon_hw.a = $(SRC_C_amd64_libcon_hw.a) ux.c
SRC_C_arm_libcon_hw.a = iomem.c
SRC_C_libcon_hw.a = $(SRC_C_$(ARCH)_libcon_hw.a)

include $(L4DIR)/mk/lib.mk

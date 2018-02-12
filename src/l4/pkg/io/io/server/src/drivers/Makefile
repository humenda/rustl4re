PKGDIR		?= ../../..
L4DIR		?= $(PKGDIR)/../../..

include $(PKGDIR)/server/src/Makefile.config

SUBDIRS                      = gpio
SUBDIRS-$(CONFIG_L4IO_ACPI) += acpi
SUBDIRS-$(CONFIG_L4IO_PCI)  += pci
SUBDIRS                     += $(SUBDIRS-y)

include $(PKGDIR)/server/src/lib_subdir.mk

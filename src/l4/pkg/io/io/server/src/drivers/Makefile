PKGDIR		?= ../../..
L4DIR		?= $(PKGDIR)/../../..

include $(PKGDIR)/server/src/Makefile.config

SUBDIRS                      = gpio
SUBDIRS-$(CONFIG_L4IO_ACPI) += acpi
SUBDIRS-$(CONFIG_L4IO_PCI)  += pci
SUBDIRS                     += $(SUBDIRS-y)

SRC_CC-$(CONFIG_L4IO_PCI)   += pcie_ecam.cc
SRC_CC-$(CONFIG_L4IO_PCI)   += pcie_rcar3.cc cpg_rcar3.cc

include $(PKGDIR)/server/src/lib_subdir.mk

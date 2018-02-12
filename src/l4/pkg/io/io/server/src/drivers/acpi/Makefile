PKGDIR		?= ../../../..
L4DIR		?= $(PKGDIR)/../../..

SUBDIRS :=
SRC_CC  := pci-root.cc \
           ec.cc       \
           button.cc   \

REQUIRES_LIBS = acpica

CPPFLAGS += -Wno-unused-parameter

include $(PKGDIR)/server/src/lib_subdir.mk

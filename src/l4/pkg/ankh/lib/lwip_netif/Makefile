PKGDIR  ?= ../..
L4DIR   ?= $(PKGDIR)/../..

PC_FILENAME   = liblwip_netif_ankh
TARGET        = liblwip_netif_ankh.a liblwip_netif_ankh.so
SYSTEMS       = x86-l4f arm-l4f
SRC_C         = ankh_if.c
REQUIRES_LIBS = lwip ankh

include $(L4DIR)/mk/lib.mk

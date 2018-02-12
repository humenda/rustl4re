PKGDIR          ?= ../..
L4DIR           ?= $(PKGDIR)/../..

TARGET           = lwip_test
SRC_C            = main.c 
SYSTEMS          = x86-l4f arm-l4f
REQUIRES_LIBS    = lwip libc_be_socket_lwip liblwip_netif_ankh libc_support_misc

include $(L4DIR)/mk/prog.mk


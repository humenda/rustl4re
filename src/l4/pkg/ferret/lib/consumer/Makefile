PKGDIR   ?= ../..
L4DIR    ?= $(PKGDIR)/../..

SRC_CC_libferret_monitor.a    = monitor.cc
SRC_C_libferret_consumer.a    = scalar_consumer.c list_consumer.c

TARGET    = libferret_monitor.a \
			libferret_consumer.a

PC_FILENAME = ferret-consumer

include $(L4DIR)/mk/lib.mk

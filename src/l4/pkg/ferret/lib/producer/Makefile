PKGDIR   ?= ../..
L4DIR    ?= $(PKGDIR)/../..

SRC_CC_libferret_client.a     = create.cc
SRC_C_libferret_init.a        = scalar_init.c list_init.c
SRC_C_libferret_producer.a    = list_producer.c

PC_FILENAME = ferret-producer

#free.c instance.c create.c create_dir.c

SYSTEMS   = x86-l4f
REQUIRES_LIBS = libboost-lite

TARGET    = libferret_client.a \
			libferret_init.a   \
			libferret_producer.a

include $(L4DIR)/mk/lib.mk

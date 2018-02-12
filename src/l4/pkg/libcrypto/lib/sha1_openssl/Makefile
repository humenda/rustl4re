PKGDIR      ?= ../..
L4DIR       ?= $(PKGDIR)/../..

TARGET       = $(PKGNAME)_sha1_openssl.a $(PKGNAME)_sha1_openssl.p.a
SYSTEMS      = x86
PC_FILENAME  = $(PKGNAME)_sha1_openssl
DEFINES     += -DBUILD_FOR_L4_LIBCRYPTO=1
SRC_C        = glue.c sha1dgst.c
SRC_S        = sx86-elf.S

include $(L4DIR)/mk/lib.mk

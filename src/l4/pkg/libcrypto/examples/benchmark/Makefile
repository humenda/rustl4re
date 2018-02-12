PKGDIR ?= ../../
L4DIR  ?= $(PKGDIR)/../..

TARGET  = crypto_bench_linux1 \
          crypto_bench_linux2 \
          crypto_bench_linux3 \
          crypto_bench_openssl
SRC_C   = bench.c modes/cbc.c
MODE    = host

SRC_C_crypto_bench_linux1  = sha1_linux/sha1.c \
                             aes_linux/aes.c
SRC_C_crypto_bench_linux2  = sha1_linux_new/sha1.c \
                             sha1_linux_new/sha1_generic.c \
                             aes_linux_586/aes.c
SRC_S_crypto_bench_linux2  = aes_linux_586/aes-i586-asm.S
SRC_C_crypto_bench_linux3  = sha1_linux_new/sha1.c \
                             sha1_linux_new/sha1_generic.c \
                             aes_linux_586_new/aes_glue.c \
                             aes_linux_586_new/aes_generic.c
SRC_S_crypto_bench_linux3  = aes_linux_586_new/aes-i586-asm_32.S
SRC_C_crypto_bench_openssl = sha1_openssl/glue.c \
                             sha1_openssl/sha1dgst.c \
                             aes_openssl/glue.c
SRC_S_crypto_bench_openssl = aes_openssl/ax86-elf.S \
                             sha1_openssl/sx86-elf.S

DEFINES += -DBUILD_FOR_L4_LIBCRYPTO=1

PRIVATE_INCDIR = $(PKGDIR)/include

vpath %.c $(PKGDIR)/lib
vpath %.S $(PKGDIR)/lib

include $(L4DIR)/mk/prog.mk

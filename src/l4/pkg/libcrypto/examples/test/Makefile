PKGDIR         ?= ../../
L4DIR          ?= $(PKGDIR)/../..

TARGET          = crypto_test_ref \
                  crypto_test_linux1 \
                  crypto_test_linux2 \
                  crypto_test_oslo \
                  crypto_test_openssl
REQUIRES_LIBS   = log
# hack, we need REQUIRES_LIBS per TARGET
PRIVATE_INCDIR  = $(PKGDIR)/include
SRC_C           = test.c
SYSTEMS         = x86-l4f

LIBS                     = -lcrypto_pad -lcrypto_rsaref2 -lcrypto_modes
LIBS_crypto_test_ref     = -lcrypto_sha1_ref -lcrypto_aes_linux.o
LIBS_crypto_test_linux1  = -lcrypto_sha1_linux -lcrypto_aes_linux.o
LIBS_crypto_test_linux2  = -lcrypto_sha1_linux_new -lcrypto_aes_linux_586.o
LIBS_crypto_test_oslo    = -lcrypto_sha1_oslo -lcrypto_aes_linux.o
LIBS_crypto_test_openssl = -lcrypto_sha1_openssl -lcrypto_aes_openssl

include $(L4DIR)/mk/prog.mk

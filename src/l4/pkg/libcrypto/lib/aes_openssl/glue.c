/*
 * \brief   Glue code for AES OpenSSL functions.
 * \date    2006-09-22
 * \author  Carsten Weinhold <weinhold@os.inf.tu-dresden.de>
 */
/*
 * Copyright (C) 2006  Carsten Weinhold <weinhold@os.inf.tu-dresden.de>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the libcrypto package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */

/* L4-specific includes */
#include <l4/crypto/aes.h>

/*
 * Unfortunately, we need wrapper functions here ...
 */

static int set_key(void *ctx, const unsigned char *key, unsigned int len, unsigned int flags) {

    int ret;
    
    ret = AES_set_encrypt_key(key, len * 8, &((crypto_aes_ctx_t *) ctx)->__aes_openssl.__enc);
    if (ret != 0)
        return ret;
    
    return AES_set_decrypt_key(key, len * 8, &((crypto_aes_ctx_t *) ctx)->__aes_openssl.__dec);
}

static void encrypt(void *ctx, unsigned char *out, const unsigned char *in) {

    AES_encrypt(in, out, &((crypto_aes_ctx_t *) ctx)->__aes_openssl.__enc);
}

static void decrypt(void *ctx, unsigned char *out, const unsigned char *in) {

    AES_decrypt(in, out, &((crypto_aes_ctx_t *) ctx)->__aes_openssl.__dec);
}

crypto_cipher_set_key_fn_t aes_cipher_set_key = (crypto_cipher_set_key_fn_t) set_key;
crypto_cipher_encrypt_fn_t aes_cipher_encrypt = (crypto_cipher_encrypt_fn_t) encrypt;
crypto_cipher_decrypt_fn_t aes_cipher_decrypt = (crypto_cipher_decrypt_fn_t) decrypt;



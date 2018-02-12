/*
 * \brief   Header AES functions.
 * \date    2006-07-26
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

#ifndef __CRYPTO_AES_H
#define __CRYPTO_AES_H

#include "private/misc.h"

CRYPTO_EXTERN_C_BEGIN

#include "private/cipher.h"
#include "private/aes_linux.h"
#include "private/aes_openssl.h"

/*
 * **************************************************************** 
 */

#define AES_BLOCK_SIZE  16

#define AES128_KEY_SIZE 16
#define AES192_KEY_SIZE 24
#define AES256_KEY_SIZE 32

/*
 * **************************************************************** 
 */

typedef union
{
    struct aes_c_ctx   __aes_linux;
    struct aes_586_ctx __aes_linux_586;
    struct crypto_aes_ctx __aes_linux_586_new;
    struct {
        AES_KEY __enc;
        AES_KEY __dec;
    } __aes_openssl;
} crypto_aes_ctx_t;

/*
 * **************************************************************** 
 */

extern crypto_cipher_set_key_fn_t aes_cipher_set_key;
extern crypto_cipher_encrypt_fn_t aes_cipher_encrypt;
extern crypto_cipher_decrypt_fn_t aes_cipher_decrypt;

CRYPTO_EXTERN_C_END

#endif /* __CRYPTO_AES_H */


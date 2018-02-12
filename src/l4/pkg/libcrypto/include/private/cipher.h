/*
 * \brief   Private header for block cipher functions.
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

#ifndef __CRYPTO_CIPHER_H
#define __CRYPTO_CIPHER_H

/*
 * **************************************************************** 
 */

#define CRYPTO_FIT_SIZE_TO_CIPHER(s, b) \
        (((s + (b - 1)) / b) * b)

/*
 * **************************************************************** 
 */

typedef int  (*crypto_cipher_set_key_fn_t)(void *ctx_arg, const char *in_key,
                                           unsigned int key_len, unsigned int *flags);
typedef void (*crypto_cipher_encrypt_fn_t)(void *ctx_arg, char *out,
                                           const char *in);
typedef void (*crypto_cipher_decrypt_fn_t)(void *ctx_arg, char *out,
                                           const char *in);

#endif /* __CRYPTO_CIPHER_H */


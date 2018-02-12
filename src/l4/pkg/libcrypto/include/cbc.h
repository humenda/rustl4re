/*
 * \brief   Header block cipher modes of operations.
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

#ifndef __CRYPTO_CBC_H
#define __CRYPTO_CBC_H

#include "private/misc.h"

CRYPTO_EXTERN_C_BEGIN

#include "private/cipher.h"

/*
 * **************************************************************** 
 */

void crypto_cbc_encrypt(crypto_cipher_encrypt_fn_t encrypt,
                        void *ctx, unsigned int block_len,
                        const char *in, char *out, const char *iv,
                        unsigned int len);
void crypto_cbc_decrypt(crypto_cipher_decrypt_fn_t decrypt,
                        void *ctx, unsigned int block_len,
                        const char *in, char *out, const char *iv,
                        unsigned int len);

CRYPTO_EXTERN_C_END

#endif /* __CRYPTO_CBC_H */


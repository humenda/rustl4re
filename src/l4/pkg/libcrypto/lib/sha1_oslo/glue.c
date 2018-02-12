/*
 * \brief   Modes of operation for block cipher functions.
 * \date    2006-08-15
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

#define SHA1_PROTOTYPES
#include <l4/crypto/sha1.h>
#include <l4/crypto/private/util.h>
#include <stdio.h>


static int sha1_final(struct Context *ctx,
                      uint8_t Message_Digest[SHA1HashSize])
{
    sha1_finish(ctx);
    memcpy(Message_Digest, ctx->hash, SHA1HashSize);
    return 0;
}


void out_string(char *value)
{
    puts(value);
}


crypto_digest_setup_fn_t  sha1_digest_setup  = (crypto_digest_setup_fn_t)  sha1_init;
crypto_digest_update_fn_t sha1_digest_update = (crypto_digest_update_fn_t) sha1;
crypto_digest_final_fn_t  sha1_digest_final  = (crypto_digest_final_fn_t)  sha1_final;

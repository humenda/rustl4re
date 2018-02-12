/*
 * \brief   Glue code for OpenSSL SHA1 functions.
 * \date    2006-09-22
 * \author  Carsten Weinhold <weinhold@os.inf.tu-dresden.de>
 */
/*
 * Copyright (C) 2006-2007  Carsten Weinhold <weinhold@os.inf.tu-dresden.de>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the libcrypto package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */

/* L4-specific includes */
#include <l4/crypto/sha1.h>

static void final(void *ctx, unsigned char *d) {
    
    SHA1_Final(d, ctx);
}

crypto_digest_setup_fn_t  sha1_digest_setup  = (crypto_digest_setup_fn_t)  SHA1_Init;
crypto_digest_update_fn_t sha1_digest_update = (crypto_digest_update_fn_t) SHA1_Update;
crypto_digest_final_fn_t  sha1_digest_final  = (crypto_digest_final_fn_t)  final;


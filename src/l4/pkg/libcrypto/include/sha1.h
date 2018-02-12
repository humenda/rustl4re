/*
 * \brief   Header SHA1 functions.
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

#ifndef __CRYPTO_SHA_H
#define __CRYPTO_SHA_H

#include "private/misc.h"

CRYPTO_EXTERN_C_BEGIN

/* generic includes */
#include <sys/types.h>

/* local includes */
#include "private/digest.h"
#include "private/sha1_linux.h"
#include "private/sha1.h"
#include "private/sha.h"
#include "private/sha_openssl.h"

/*
 * **************************************************************** 
 */

/* SHA-1 uses 160 bit hashes */
#define SHA1_DIGEST_SIZE 20

/*
 * **************************************************************** 
 */

typedef union
{
    SHA1Context       __sha1_ref_ctx;
    struct sha1_ctx   __sha1_linux_ctx;    
    struct shash_desc __sha1_linux_new_ctx;
    struct Context    __sha1_oslo_ctx;
    SHA_CTX           __sha1_openssl;
} crypto_sha1_ctx_t;

typedef char crypto_sha1_digest_t[SHA1_DIGEST_SIZE];

/*
 * **************************************************************** 
 */

extern crypto_digest_setup_fn_t  sha1_digest_setup;
extern crypto_digest_update_fn_t sha1_digest_update;
extern crypto_digest_final_fn_t  sha1_digest_final;

CRYPTO_EXTERN_C_END

#endif /* __CRYPTO_SHA_H */


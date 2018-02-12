/*
 * \brief   Private header for message digest functions.
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

#ifndef __CRYPTO_DIGEST_H
#define __CRYPTO_DIGEST_H

/*
 * **************************************************************** 
 */

typedef void (*crypto_digest_setup_fn_t) (void *ctx);
typedef void (*crypto_digest_update_fn_t)(void *ctx, const char *data,
                                          unsigned int len);
typedef void (*crypto_digest_final_fn_t) (void *ctx, char *out);

#endif /* __CRYPTO_DIGEST_H */


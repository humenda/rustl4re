/*
 * \brief   Header file for the OAEP padding method (see PKCS #1 v2.1: RSA Cryptography Standard)
 * \date    2006-07-05
 * \author  Carsten Weinhold <weinhold@os.inf.tu-dresden.de>
 */
/*
 * Copyright (C) 2006 Carsten Weinhold <weinhold@os.inf.tu-dresden.de>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the libcrypto package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */

#ifndef __PAD_OAEP_H
#define __PAD_OAEP_H

#include "private/misc.h"

CRYPTO_EXTERN_C_BEGIN

int mgf1(const char *seed, unsigned seed_len,
         char *mask_out, unsigned mask_len);

int pad_oaep(const char *msg, unsigned msg_len,
             const char *label, unsigned label_len,
             unsigned mod_len, char *enc_msg_out);

CRYPTO_EXTERN_C_END

#endif /* __PAD_OAEP_H */

/*
 * \brief   Header file for the OAEP padding method (see PKCS #1 v2.1: RSA Cryptography Standard)
 * \date    2006-07-05
 * \author  Carsten Weinhold <weinhold@os.inf.tu-dresden.de>
 */
/*
 * Copyright (C) 2006 Carsten Weinhold <weinhold@os.inf.tu-dresden.de>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the STPM package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */

#ifndef __CRYPTO_RANDOM_H
#define __CRYPTO_RANDOM_H

#include "private/misc.h"

CRYPTO_EXTERN_C_BEGIN

/*
 * Fill the buffer buf with buf_len random bytes. You must provide an
 * implementation for this function.
 */
int crypto_randomize_buf(char *buf, unsigned int buf_len);

CRYPTO_EXTERN_C_END

#endif /* __CRYPTO_RANDOM_H */

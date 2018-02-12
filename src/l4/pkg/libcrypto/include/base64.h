/*
 * \brief   Header base64 functions.
 * \date    2005-09-12
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

#ifndef __CRYPTO_BASE64_H
#define __CRYPTO_BASE64_H

#include "private/misc.h"

CRYPTO_EXTERN_C_BEGIN

/*
 * *****************************************************************
 */

#define BASE64_LEN(l) ((l * 8 + 5) / 6)

/*
 * *****************************************************************
 */

/* BIG FAT WARNING: This is not a standard-conforming base64 encoder;
 *                  the padding is incorrect (and I don't care)! */

int crypto_base64_encode(const char *in, char *out, unsigned int len);

CRYPTO_EXTERN_C_END

#endif /* __CRYPTO_BASE64_H */



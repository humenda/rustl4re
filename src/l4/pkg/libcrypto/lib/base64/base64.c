/*
 * \brief   Base64 functions.
 * \date    2005-09-12
 * \author  Carsten Weinhold <weinhold@os.inf.tu-dresden.de>
 */
/*
 * Copyright (C) 2005-2007  Carsten Weinhold <weinhold@os.inf.tu-dresden.de>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the libcrypto package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */

/* generic includes */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <l4/crypto/base64.h>

/*
 * *****************************************************************
 */

static unsigned char dtable[64];

/*
 * *****************************************************************
 */

int crypto_base64_encode(const char *in, char *out, unsigned int len) {

    /* this not really base64 */

    unsigned int pos = 0;
    unsigned char igroup[3];

    while (pos < len) {

        igroup[0] = in[pos];
        if (pos + 2 < len) {
            igroup[1] = in[pos + 1];
            igroup[2] = in[pos + 2];
        } else {
            igroup[1] = (pos + 1 < len) ? in[pos + 1] : 0;
            igroup[2] = 0;
        }

        out[0] = dtable[  igroup[0] >> 2];
        out[1] = dtable[((igroup[0] & 3)   << 4) | (igroup[1] >> 4)];
        out[2] = dtable[((igroup[1] & 0xF) << 2) | (igroup[2] >> 6)];
        out[3] = dtable[  igroup[2] & 0x3F];

        pos += 3;
        out += 4;
    }
  
    return BASE64_LEN(len);
}

/*
 * *****************************************************************
 */

static void init(void) __attribute__((constructor));
static void init(void) {

    unsigned int i;

    for (i = 0; i < 9; i++) {    
        dtable[     i    ] = 'A' + i;
        dtable[     i + 9] = 'J' + i;
        dtable[26 + i    ] = 'a' + i;
        dtable[26 + i + 9] = 'j' + i;
    }
    for (i = 0; i < 8; i++) {
        dtable[     i + 18] = 'S' + i;
        dtable[26 + i + 18] = 's' + i;
    }
    for (i = 0; i < 10; i++) {
        dtable[52 + i] = '0' + i;
    }
    dtable[62] = '+';
    dtable[63] = '-';
}



/*
 * \brief   Modes of operation for block cipher functions.
 * \date    2005-09-22
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

/* general includes */
#include <string.h>

/* L4-specific includes */
#include <stdint.h>

/* local includes */
#include <l4/crypto/cbc.h>

/*
 * *****************************************************************
 */

#define XOR_ROUNDS ( / sizeof(uint32_t))

/*
 * *****************************************************************
 */

void crypto_cbc_encrypt(crypto_cipher_encrypt_fn_t encrypt,
                        void *ctx, unsigned int block_len,
                        const char *in, char *out,
                        const char *iv, unsigned int len) {

    unsigned int pos, i;
    const unsigned int xor_rounds = block_len / sizeof(uint32_t);
    uint32_t *iv32, *in32;
    uint32_t  xor32_buf[xor_rounds];
    
    //Assert(len % TFS_CIPHER_BLOCK_SIZE == 0);
    
    iv32 = (uint32_t *) iv;
    
    for (pos = 0; pos < len; pos += block_len) {
        
        in32 = (uint32_t *) &in[pos];
        
        for (i = 0; i < xor_rounds; i++)
            xor32_buf[i] = in32[i] ^ iv32[i];
        encrypt(ctx, &out[pos], (char *) xor32_buf);
        
        iv32 = (uint32_t *) &out[pos];
    }  
}


void crypto_cbc_decrypt(crypto_cipher_decrypt_fn_t decrypt,
                        void *ctx, unsigned int block_len,
                        const char *in, char *out,
                        const char *iv, unsigned int len) {    

    unsigned int pos, i;
    const unsigned int xor_rounds = block_len / sizeof(uint32_t);
    uint32_t *iv32, *out32;
    uint32_t  xor32_buf[xor_rounds];
    
    //Assert(len % TFS_CIPHER_BLOCK_SIZE == 0);

    iv32 = (uint32_t *) iv;
    
    for (pos = 0; pos < len; pos += block_len) {
        
        out32 = (uint32_t *) &out[pos];
        
        decrypt(ctx, (char *) xor32_buf, &in[pos]);
        for (i = 0; i < xor_rounds; i++)
            out32[i] = xor32_buf[i] ^ iv32[i];
        
        iv32 = (uint32_t *) &in[pos];
    }  
}





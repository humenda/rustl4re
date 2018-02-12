/*
 * \brief   Implementation of the OAEP padding method (see PKCS #1 v2.1: RSA Cryptography Standard)
 * \date    2006-07-04
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <l4/crypto/sha1.h>
#include <l4/crypto/oaep.h>
#include <l4/crypto/random.h>

/*
 * ***************************************************************************
 */

int mgf1(const char *seed, unsigned seed_len,
         char *mask_out, unsigned mask_len) {

    int i, left = mask_len;
    int n = (mask_len + SHA1_DIGEST_SIZE - 1) / SHA1_DIGEST_SIZE;
    crypto_sha1_ctx_t ctx;

    for (i = 0; i < n; i++) {

        char ioc[4];
        char hash[SHA1_DIGEST_SIZE];
        
        /* convert counter 'i' into big-endian octet string */
        ioc[0] = (i & 0xff000000) >> 24;
        ioc[1] = (i & 0x00ff0000) >> 16;
        ioc[2] = (i & 0x0000ff00) >> 8;
        ioc[3] = (i & 0x000000ff) >> 0;

        /* append SHA1(seed,ioc) to mask_out */
        sha1_digest_setup(&ctx);
        sha1_digest_update(&ctx, seed, seed_len);
        sha1_digest_update(&ctx, ioc, 4);
        sha1_digest_final(&ctx, hash);

        memcpy(mask_out, hash, (left >= SHA1_DIGEST_SIZE) ? SHA1_DIGEST_SIZE : left);
        left     -= SHA1_DIGEST_SIZE;
        mask_out += SHA1_DIGEST_SIZE;
    }

    return 1;
}


static void xor_bytes(const char *a, const char *b, char *xor_out,
                      unsigned len) {

    unsigned int i;

    for (i = 0; i < len; i++)
        xor_out[i] = a[i] ^ b[i];
}


int pad_oaep(const char *msg, unsigned msg_len,
             const char *label, unsigned label_len,
             unsigned mod_len, char *enc_msg_out) {

    const unsigned seed_len  = SHA1_DIGEST_SIZE;
    const unsigned db_len    = mod_len - SHA1_DIGEST_SIZE - 1;
    const unsigned num_zeros = mod_len - msg_len - 2 * SHA1_DIGEST_SIZE - 2;

    char *db, *db_mask, *db_ptr;
    char seed_mask[seed_len];
    char seed[seed_len];
    crypto_sha1_ctx_t ctx;

    if ((unsigned)crypto_randomize_buf(seed, seed_len) != seed_len)
        return -1; /* not enough randomness */

    /* check arguments and alloc buffers */
    if (msg == NULL || enc_msg_out == NULL ||
        (label == NULL && label_len != 0))
        return -1;

    if (msg_len > mod_len - 2 * SHA1_DIGEST_SIZE - 2)
        return -1; /* msg too long */

    db = malloc(db_len);
    if (db == NULL)
        return -1; /* out of memory */

    db_mask = malloc(db_len);
    if (db_mask == NULL) {
        free(db);
        return -1; /* out of memory */
    }

    /* concatenate the individual components to form 'db':
     * db := H(label) || 0x00...0x00 || 0x01 || msg */

    db_ptr = db;
    sha1_digest_setup(&ctx);
    if (label_len > 0)
        sha1_digest_update(&ctx, label, label_len);
    sha1_digest_final(&ctx, db_ptr);

    db_ptr += SHA1_DIGEST_SIZE;
    memset(db_ptr, 0, num_zeros);

    db_ptr += num_zeros;
    *db_ptr = 0x01;

    db_ptr++;
    memcpy(db_ptr, msg, msg_len);

    /* now build the encoded message 'enc_msg_out':
     * enc_msg_out := 0x00 || masked_seed || masked_db */

    /* FIXME: randomize seed here! */

    *enc_msg_out = 0x00;

    /* create masked db first */
    mgf1(seed, seed_len, db_mask, db_len);
    xor_bytes(db, db_mask, enc_msg_out + 1 + seed_len, db_len);

    /* build masked seed using masked db */
    mgf1(enc_msg_out + 1 + seed_len, db_len, seed_mask, seed_len);
    xor_bytes(seed, seed_mask, enc_msg_out + 1, seed_len);

    free(db);
    free(db_mask);

    return 0;
}


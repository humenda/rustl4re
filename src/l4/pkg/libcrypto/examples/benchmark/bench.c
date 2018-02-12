/*
 * \brief   Quick'n'dirty benchmarks for libcrypto
 * \date    2013-05-07
 * \author  Carsten Weinhold <weinhold@os.inf.tu-dresden.de>
 */
/*
 * Copyright (C) 2013 Carsten Weinhold <weinhold@os.inf.tu-dresden.de>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the libcrypto package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "sha1.h"
#include "aes.h"
#include "cbc.h"

/*
 * ***************************************************************************
 */

static void print_hex(const char *bytes, int len) {

    int i;

    for (i = 0; i < len; i++) {
        unsigned int n = (unsigned char) bytes[i];
        printf("%02x ", n);
        if (i % 16 == 15)
            printf("\n");
    }
    printf("\n");
}

/*
 * ***************************************************************************
 */

static char src_buf[4096*25600];
static char dst_buf[4096*25600];

/*
 * ***************************************************************************
 */

static void print_benchmark_result(struct timeval *tv0, struct timeval *tv1,
                                   char const *label, size_t buf_size) {

    uint64_t usecs = (tv1->tv_sec * 1000000 + tv1->tv_usec) -
                     (tv0->tv_sec * 1000000 + tv0->tv_usec);

    printf("%s: %llu us, %.2f MB/s\n", label, usecs,
           (float)buf_size / 1048576 / ((float)usecs / 1000000));
}

/*
 * ***************************************************************************
 */

static int bench_aes_sha1(void) {

    /* RFC 3602: Test vectors, case #4:
     * Encrypting 64 bytes (4 blocks) using AES-CBC with 128-bit key
     */
    
    const char key[] =
        { 0x56, 0xe4, 0x7a, 0x38, 0xc5, 0x59, 0x89, 0x74,
          0xbc, 0x46, 0x90, 0x3d, 0xba, 0x29, 0x03, 0x49 };
    const char iv[] =
        { 0x8c, 0xe8, 0x2e, 0xef, 0xbe, 0xa0, 0xda, 0x3c,
          0x44, 0x69, 0x9e, 0xd7, 0xdb, 0x51, 0xb7, 0xd9 };

    crypto_aes_ctx_t aes_ctx;

    memset(src_buf, 0, sizeof(src_buf));
    memset(dst_buf, 0, sizeof(src_buf));

    aes_cipher_set_key(&aes_ctx, key, AES128_KEY_SIZE, 0);

    crypto_sha1_ctx_t sha1_ctx;
    char hash1[SHA1_DIGEST_SIZE];
    char hash2[SHA1_DIGEST_SIZE];

    /* warmup code */
    crypto_cbc_encrypt(aes_cipher_encrypt, &aes_ctx, AES_BLOCK_SIZE,
                       src_buf, dst_buf, iv, 4096);
    
    crypto_cbc_decrypt(aes_cipher_decrypt, &aes_ctx, AES_BLOCK_SIZE,
                       dst_buf, src_buf, iv, 4096);

    sha1_digest_setup(&sha1_ctx);
    sha1_digest_update(&sha1_ctx, src_buf, 4096);
    sha1_digest_final(&sha1_ctx, hash1);


    struct timeval tv0, tv1, tv2, tv3, tv4;

    gettimeofday(&tv0, NULL);

    sha1_digest_setup(&sha1_ctx);
    sha1_digest_update(&sha1_ctx, src_buf, sizeof(src_buf));
    sha1_digest_final(&sha1_ctx, hash1);

    gettimeofday(&tv1, NULL);

    crypto_cbc_encrypt(aes_cipher_encrypt, &aes_ctx, AES_BLOCK_SIZE,
                       src_buf, dst_buf, iv, sizeof(dst_buf));
    
    gettimeofday(&tv2, NULL);

    crypto_cbc_decrypt(aes_cipher_decrypt, &aes_ctx, AES_BLOCK_SIZE,
                       dst_buf, src_buf, iv, sizeof(src_buf));

    gettimeofday(&tv3, NULL);

    sha1_digest_setup(&sha1_ctx);
    sha1_digest_update(&sha1_ctx, src_buf, sizeof(src_buf));
    sha1_digest_final(&sha1_ctx, hash2);

    gettimeofday(&tv4, NULL);

    print_benchmark_result(&tv0, &tv1, "sha1 pre  ", sizeof(src_buf));
    print_benchmark_result(&tv1, &tv2, "aes128 enc", sizeof(src_buf));
    print_benchmark_result(&tv2, &tv3, "aes128 dec", sizeof(src_buf));
    print_benchmark_result(&tv3, &tv4, "sha1 post ", sizeof(src_buf));

    print_hex(hash1, SHA1_DIGEST_SIZE);
    print_hex(hash2, SHA1_DIGEST_SIZE);

    return memcmp(hash1, hash2, SHA1_DIGEST_SIZE);
}

/*
 * ***************************************************************************
 */

int main(void) {

    printf("bench_aes_sha1(): %d (0 expected)\n", bench_aes_sha1());

    return 0;
}


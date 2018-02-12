/*
 * \brief   Private header Linux SHA1 functions.
 * \date    2006-07-26
 * \author  Carsten Weinhold <weinhold@os.inf.tu-dresden.de>
 */
/*
 * Copyright (C) 2006-2012  Carsten Weinhold <weinhold@os.inf.tu-dresden.de>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the libcrypto package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */

#ifndef __CRYPTO_SHA1_LINUX_H
#define __CRYPTO_SHA1_LINUX_H

#include "linux.h"

/* all code below has been copied from Linux 3.3.0 sources (GPLv2) */

#define SHA1_DIGEST_SIZE        20
#define SHA1_BLOCK_SIZE         64

#define SHA224_DIGEST_SIZE	28
#define SHA224_BLOCK_SIZE	64

#define SHA256_DIGEST_SIZE      32
#define SHA256_BLOCK_SIZE       64

#define SHA384_DIGEST_SIZE      48
#define SHA384_BLOCK_SIZE       128

#define SHA512_DIGEST_SIZE      64
#define SHA512_BLOCK_SIZE       128

#define SHA1_H0		0x67452301UL
#define SHA1_H1		0xefcdab89UL
#define SHA1_H2		0x98badcfeUL
#define SHA1_H3		0x10325476UL
#define SHA1_H4		0xc3d2e1f0UL

struct sha1_ctx {
	u64 count;
	u32 state[SHA1_DIGEST_SIZE / 4];
	u8 buffer[SHA1_BLOCK_SIZE];
};

struct sha1_state {
	u64 count;
	u32 state[SHA1_DIGEST_SIZE / 4];
	u8 buffer[SHA1_BLOCK_SIZE];
};

#define CRYPTO_MINALIGN_ATTR __attribute__ ((__aligned__(16)))

struct hash_tfm {
#if 0
	int (*init)(struct hash_desc *desc);
	int (*update)(struct hash_desc *desc,
		      struct scatterlist *sg, unsigned int nsg);
	int (*final)(struct hash_desc *desc, u8 *out);
	int (*digest)(struct hash_desc *desc, struct scatterlist *sg,
		      unsigned int nsg, u8 *out);
	int (*setkey)(struct crypto_hash *tfm, const u8 *key,
		      unsigned int keylen);
	unsigned int digestsize;
#endif
};

struct crypto_alg;

struct crypto_tfm {

	u32 crt_flags;
	
	union {
		//struct ablkcipher_tfm ablkcipher;
		//struct aead_tfm aead;
		//struct blkcipher_tfm blkcipher;
		//struct cipher_tfm cipher;
		struct hash_tfm hash;
		//struct compress_tfm compress;
		//struct rng_tfm rng;
	} crt_u;

	void (*exit)(struct crypto_tfm *tfm);
	
	struct crypto_alg *__crt_alg;

	void *__crt_ctx[] CRYPTO_MINALIGN_ATTR;
};

struct crypto_shash {
	unsigned int descsize;
	struct crypto_tfm base;
};

struct shash_desc {
	struct crypto_shash *tfm;
	u32 flags;

	void *__ctx[] CRYPTO_MINALIGN_ATTR;
};

static inline void *shash_desc_ctx(struct shash_desc *desc)
{
	return desc->__ctx;
}

#endif /* __CRYPTO_SHA1_LINUX_H */


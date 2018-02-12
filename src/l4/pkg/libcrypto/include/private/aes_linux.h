/*
 * \brief   Private header Linux AES functions.
 * \date    2006-07-26
 * \author  Carsten Weinhold <weinhold@os.inf.tu-dresden.de>
 */
/*
 * Copyright (C) 2006-2007  Carsten Weinhold <weinhold@os.inf.tu-dresden.de>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the libcrypto package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */

#ifndef __CRYPTO_AES_LINUX_H
#define __CRYPTO_AES_LINUX_H

#include "linux.h"

/* AES C implementation */

struct aes_c_ctx {
	int key_length;
	u32 E[60];
	u32 D[60];
};

/* AES i586 ASM implementation */

#define AES_MIN_KEY_SIZE	16
#define AES_MAX_KEY_SIZE	32
#define AES_BLOCK_SIZE		16
#define AES_KS_LENGTH		4 * AES_BLOCK_SIZE
#define RC_LENGTH		29

struct aes_586_ctx {
	u32 ekey[AES_KS_LENGTH];
	u32 rounds;
	u32 dkey[AES_KS_LENGTH];
};
#endif /* __CRYPTO_AES_LINUX_H */

#define AES_MIN_KEY_SIZE	16
#define AES_MAX_KEY_SIZE	32
#define AES_KEYSIZE_128		16
#define AES_KEYSIZE_192		24
#define AES_KEYSIZE_256		32
#define AES_BLOCK_SIZE		16
#define AES_MAX_KEYLENGTH	(15 * 16)
#define AES_MAX_KEYLENGTH_U32	(AES_MAX_KEYLENGTH / sizeof(u32))

/*
 * Please ensure that the first two fields are 16-byte aligned
 * relative to the start of the structure, i.e., don't move them!
 */
struct crypto_aes_ctx {
	u32 key_enc[AES_MAX_KEYLENGTH_U32];
	u32 key_dec[AES_MAX_KEYLENGTH_U32];
	u32 key_length;
};

extern const u32 crypto_ft_tab[4][256];
extern const u32 crypto_fl_tab[4][256];
extern const u32 crypto_it_tab[4][256];
extern const u32 crypto_il_tab[4][256];

int crypto_aes_expand_key(struct crypto_aes_ctx *ctx, const u8 *in_key,
		unsigned int key_len);
void crypto_aes_encrypt_x86(struct crypto_aes_ctx *ctx, u8 *dst,
			    const u8 *src);
void crypto_aes_decrypt_x86(struct crypto_aes_ctx *ctx, u8 *dst,
			    const u8 *src);

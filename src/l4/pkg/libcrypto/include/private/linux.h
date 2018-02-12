/*
 * \brief   Private header for Linux compatibility layer.
 * \date    2005-10-15
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

#ifndef __TFS_LINUX_H
#define __TFS_LINUX_H

/* ************************************************************** */

#include <stdint.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef uint8_t  __u8;
typedef uint16_t __u16;
typedef uint32_t __u32;
typedef uint64_t __u64;

typedef __u32 __be32;
typedef __u64 __be64;

#define EXPORT_SYMBOL(x)

#ifdef __LIBCRYPTO_INTERNAL__

#define __u32 u32
#define __le32 u32

/* ************************************************************** */

#define __initdata
#define __init

/* ************************************************************** */

/* FIXME: this is only for little-endian systems */

#define le32_to_cpu(x) ((u32)(x))
#define cpu_to_le32(x) ((u32)(x))

/* borrowed from <linux/byteorder/swab.h> from Linux 2.6.11 */
#define ___swab32(x) \
({ \
        __u32 __x = (x); \
        ((__u32)( \
                (((__u32)(__x) & (__u32)0x000000ffUL) << 24) | \
                (((__u32)(__x) & (__u32)0x0000ff00UL) <<  8) | \
                (((__u32)(__x) & (__u32)0x00ff0000UL) >>  8) | \
                (((__u32)(__x) & (__u32)0xff000000UL) >> 24) )); \
})

#define ___swab64(x) ((__u64)(				\
	(((__u64)(x) & (__u64)0x00000000000000ffULL) << 56) |	\
	(((__u64)(x) & (__u64)0x000000000000ff00ULL) << 40) |	\
	(((__u64)(x) & (__u64)0x0000000000ff0000ULL) << 24) |	\
	(((__u64)(x) & (__u64)0x00000000ff000000ULL) <<  8) |	\
	(((__u64)(x) & (__u64)0x000000ff00000000ULL) >>  8) |	\
	(((__u64)(x) & (__u64)0x0000ff0000000000ULL) >> 24) |	\
	(((__u64)(x) & (__u64)0x00ff000000000000ULL) >> 40) |	\
	(((__u64)(x) & (__u64)0xff00000000000000ULL) >> 56)))

#define be32_to_cpu(x) ___swab32(x)
#define be64_to_cpu(x) ___swab64(x)
#define cpu_to_be32(x) ___swab32(x)
#define cpu_to_be64(x) ___swab64(x)

/* ************************************************************** */

//#define asmlinkage extern "C" 
#define asmlinkage

/* ************************************************************** */

/* taken from <linux/crypto.h> from Linux 2.6.11 */
#define CRYPTO_TFM_RES_BAD_KEY_LEN      0x00200000

/* ************************************************************** */

/* FIXME: is there a more appropriate value? */
#ifndef EINVAL
#define EINVAL -1
#endif

/* ************************************************************** */

/* BEGIN: copied from Linux 3.3.0 sources (GPLv2) */

/**
 * rol32 - rotate a 32-bit value left
 * @word: value to rotate
 * @shift: bits to roll
 */
static inline __u32 rol32(__u32 word, unsigned int shift)
{
	return (word << shift) | (word >> (32 - shift));
}

/**
 * ror32 - rotate a 32-bit value right
 * @word: value to rotate
 * @shift: bits to roll
 */
static inline __u32 ror32(__u32 word, unsigned int shift)
{
	return (word >> shift) | (word << (32 - shift));
}

static inline u32 __get_unaligned_be32(const u8 *p)
{
	return p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];
}
static inline u32 get_unaligned_be32(const u32 *p)
{
    return __get_unaligned_be32((const u8 *)p);
}

/* END: copied from Linux 3.3.0 sources (GPLv2) */

#endif /* __LIBCRYPTO_INTERNAL__ */

#endif /* __TFS_LINUX_H */


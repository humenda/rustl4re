/*
 * \brief  Blitting function for x86
 * \author Norman Feske
 * \date   2007-10-09
 */
/*
 * (c) 2007 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/mag-gfx/blit>

namespace Mag_gfx { namespace Blit {


/***************
 ** Utilities **
 ***************/


/**
 * Copy single 16bit column
 */
static inline void copy_unaligned_column(char const *src, int src_w,
                                         char *dst, int dst_w, int w, int h)
{
  typedef struct { char x[3]; } __attribute__((packed)) _3byte;
  switch (w)
    {
    case 1:
      for (; h-- > 0; src += src_w, dst += dst_w)
	*(char *)dst = *(char const *)src;
      break;
    case 2:
      for (; h-- > 0; src += src_w, dst += dst_w)
	*(short *)dst = *(short const *)src;
      break;
    case 3:
      for (; h-- > 0; src += src_w, dst += dst_w)
	*(_3byte *)dst = *(_3byte const *)src;
      break;
    }
}


/**
 * Copy pixel block 32bit-wise
 *
 * \param src    Source address
 * \param dst    32bit-aligned destination address
 * \param w      Number of 32bit words to copy per line
 * \param h      Number of lines to copy
 * \param src_w  Width of source buffer in bytes
 * \param dst_w  Width of destination buffer in bytes
 */
static inline void copy_block_32bit(char const *src, int src_w,
                                    char *dst, int dst_w,
                                    int w, int h)
{
	long d0, d1, d2;

	asm volatile ("cld; mov %%ds, %%ax; mov %%ax, %%es" : : : "eax");
	for (; h--; src += src_w, dst += dst_w )
		asm volatile ("rep movsl"
		 : "=S" (d0), "=D" (d1), "=c" (d2)
		 : "S" (src), "D" (dst), "c" (w));
}


/**
 * Copy 32byte chunks via MMX
 */
static inline void copy_32byte_chunks(void const *src, void *dst, int size)
{
	asm volatile (
		"emms                             \n\t"
		"xor    %%ecx,%%ecx               \n\t"
		".align 16                        \n\t"
		"0:                               \n\t"
		"movq   (%%esi,%%ecx,8),%%mm0     \n\t"
		"movq   8(%%esi,%%ecx,8),%%mm1    \n\t"
		"movq   16(%%esi,%%ecx,8),%%mm2   \n\t"
		"movq   24(%%esi,%%ecx,8),%%mm3   \n\t"
		"movntq %%mm0,(%%edi,%%ecx,8)     \n\t"
		"movntq %%mm1,8(%%edi,%%ecx,8)    \n\t"
		"movntq %%mm2,16(%%edi,%%ecx,8)   \n\t"
		"movntq %%mm3,24(%%edi,%%ecx,8)   \n\t"
		"add    $4,%%ecx                  \n\t"
		"dec    %0                        \n\t"
		"jnz    0b                        \n\t"
		"sfence                           \n\t"
		"emms                             \n\t"
		: "=r" (size)
		: "S" (src), "D" (dst), "0" (size)
		: "ecx", "memory", "cc"
	);
}


/**
 * Copy block with a size of multiple of 32 bytes
 *
 * \param w  Width in 32 byte chunks to copy per line
 * \param h  Number of lines of copy
 */
static inline void copy_block_32byte(char const *src, int src_w,
                                     char *dst, int dst_w,
                                     int w, int h)
{
	if (w)
		for (int i = h; i--; src += src_w, dst += dst_w)
			copy_32byte_chunks(src, dst, w);
}


/***********************
 ** Library interface **
 ***********************/

void blit(void const *s, unsigned src_w,
          void *d, unsigned dst_w,
          int w, int h)
{
	char const *src = (char const *)s;
	char *dst = (char *)d;

	if (w <= 0 || h <= 0) return;

	/* copy unaligned column */
	if (w && ((long)dst & 3)) {
		copy_unaligned_column(src, src_w, dst, dst_w, w & 3, h);
		src += w & 3; dst += w & 3; w &= ~3;
	}

	/* now, we are on a 32bit aligned destination address */

	/* copy 32byte chunks */
	if (w >> 5) {
		copy_block_32byte(src, src_w, dst, dst_w, w >> 5, h);
		src += w & ~31;
		dst += w & ~31;
		w    = w &  31;
	}

	/* copy 32bit chunks */
	if (w >> 2) {
		copy_block_32bit(src, src_w, dst, dst_w, w >> 2, h);
		src += w & ~3;
		dst += w & ~3;
		w    = w &  3;
	}

	/* handle trailing row */
	if (w)
	  copy_unaligned_column(src, src_w, dst, dst_w, w, h);
}

}}

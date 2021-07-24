/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2020 Kernkonzept GmbH.
 * Author(s): Frank Mehnert <frank.mehnert@kernkonzept.com>
 */

#include <stddef.h>
#include <l4/sys/l4int.h>

#include "memcpy_aligned.h"

/**
 * Optimized memcpy for working on aligned memory using bigger memory chunks.
 * This improves the copy performance if cache / MMU are not enabled. For
 * instance, on ARM64 the compiler creates a nicely optimized loop using stp /
 * ldp instructions. Like with normal memcpy, the source and the destination
 * regions must not overlap.
 *
 * \param dst_aligned  Destination address. Must be at least 8-byte aligned!
 * \param src_aligned  Source address. Must be at least 8-byte aligned!
 * \param size         Number of bytes to copy. No alignment required.
 */
void memcpy_aligned(void *dst_aligned, void const *src_aligned, size_t size)
{
  l4_uint64_t const *s8 = (l4_uint64_t const *)src_aligned;
  l4_uint64_t *d8 = (l4_uint64_t *)dst_aligned;
  size_t sz8 = (size / 8) & ~7;
  for (size_t i = 0; i < sz8; i += 8)
    {
      l4_uint64_t a0 = s8[i];
      l4_uint64_t a1 = s8[i + 1];
      l4_uint64_t a2 = s8[i + 2];
      l4_uint64_t a3 = s8[i + 3];
      l4_uint64_t a4 = s8[i + 4];
      l4_uint64_t a5 = s8[i + 5];
      l4_uint64_t a6 = s8[i + 6];
      l4_uint64_t a7 = s8[i + 7];
      d8[i]     = a0;
      d8[i + 1] = a1;
      d8[i + 2] = a2;
      d8[i + 3] = a3;
      d8[i + 4] = a4;
      d8[i + 5] = a5;
      d8[i + 6] = a6;
      d8[i + 7] = a7;
    }

  s8 += sz8;
  d8 += sz8;
  sz8 = (size / 8) & 7;
  for (size_t i = 0; i < sz8; ++i)
    d8[i] = s8[i];

  l4_uint8_t const *s1 = (l4_uint8_t const *)(s8 + sz8);
  l4_uint8_t *d1 = (l4_uint8_t *)(d8 + sz8);
  for (size_t i = 0; i < size % 8; ++i)
    d1[i] = s1[i];
}

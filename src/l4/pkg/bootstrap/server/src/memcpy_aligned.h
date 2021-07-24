/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2020 Kernkonzept GmbH.
 * Author(s): Frank Mehnert <frank.mehnert@kernkonzept.com>
 */

#pragma once

#ifdef __cplusplus
extern "C"
#endif
void memcpy_aligned(void *dst, void const *src, size_t size);

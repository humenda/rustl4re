/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#ifndef __BOOTSTRAP__UNCOMPRESS_H__
#define __BOOTSTRAP__UNCOMPRESS_H__

#include <l4/sys/compiler.h>

EXTERN_C_BEGIN

void *decompress(const char *name, void *start, void *destbuf,
                 int size, int size_uncompressed);

EXTERN_C_END

#endif /* ! __BOOTSTRAP__UNCOMPRESS_H__ */

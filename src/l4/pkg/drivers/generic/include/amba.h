/*
 * (c) 2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU Lesser General Public License 2.1.
 * Please see the COPYING-LGPL-2.1 file for details.
 */
#ifndef __ARM_DRIVERS__GENERIC__INCLUDE__AMBA_H__
#define __ARM_DRIVERS__GENERIC__INCLUDE__AMBA_H__

#include <sys/cdefs.h>
#include <l4/sys/types.h>
#include <stdint.h>

EXTERN_C_BEGIN

void amba_read_id(l4_addr_t address, uint32_t *periphid, uint32_t *cellid);

EXTERN_C_END

#endif /* ! __ARM_DRIVERS__LCD__INCLUDE__LCD_H__ */

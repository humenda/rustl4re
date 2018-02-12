/*
 * This file is part of DDEKit.
 *
 * (c) 2006-2012 Bjoern Doebel <doebel@os.inf.tu-dresden.de>
 *               Christian Helmuth <ch12@os.inf.tu-dresden.de>
 *               Thomas Friebel <tf13@os.inf.tu-dresden.de>
 *     economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#pragma once

#include <l4/sys/compiler.h>
#include <l4/dde/ddekit/types.h>

EXTERN_C_BEGIN

int ddekit_request_dma(int nr);
int ddekit_release_dma(int nr);
int ddekit_request_io (ddekit_addr_t start, ddekit_addr_t count);
int ddekit_release_io (ddekit_addr_t start, ddekit_addr_t count);
int ddekit_request_mem(ddekit_addr_t start, ddekit_addr_t count, ddekit_addr_t *vaddr);
int ddekit_release_mem(ddekit_addr_t start, ddekit_addr_t count);

EXTERN_C_END

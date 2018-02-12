/*!
 * \file	iomem.h
 * \brief	I/O memory stuff
 *
 * \date	07/2002
 * \author	Frank Mehnert <fm3@os.inf.tu-dresden.de> */
/*
 * (c) 2002-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#ifndef __IOMEM_H_
#define __IOMEM_H_

int  map_io_mem(l4_addr_t addr, l4_size_t size, int cacheable,
		const char *id, l4_addr_t *vaddr);

void unmap_io_mem(l4_addr_t addr, l4_size_t size,
		  const char *id, l4_addr_t vaddr);

#endif

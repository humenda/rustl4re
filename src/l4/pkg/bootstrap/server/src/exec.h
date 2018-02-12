/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>,
 *               Frank Mehnert <fm3@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#ifndef EXEC_H
#define EXEC_H

#include "types.h"
#include <l4/sys/compiler.h>
#include <l4/util/mb_info.h>

typedef int exec_sectype_t;

#define EXEC_SECTYPE_READ		((exec_sectype_t)0x000001)
#define EXEC_SECTYPE_WRITE		((exec_sectype_t)0x000002)
#define EXEC_SECTYPE_EXECUTE		((exec_sectype_t)0x000004)
#define EXEC_SECTYPE_ALLOC		((exec_sectype_t)0x000100)
#define EXEC_SECTYPE_LOAD		((exec_sectype_t)0x000200)
#define EXEC_SECTYPE_KIP		((exec_sectype_t)0x100000)
#define EXEC_SECTYPE_KOPT		((exec_sectype_t)0x110000)
#define EXEC_SECTYPE_TYPE_MASK		((exec_sectype_t)0xff0000)

typedef int exec_handler_func_t(void *handle,
				  l4_addr_t file_ofs, l4_size_t file_size,
				  l4_addr_t mem_addr, l4_addr_t v_addr,
				  l4_size_t mem_size,
				  exec_sectype_t section_type);

EXTERN_C_BEGIN

int exec_load_elf(exec_handler_func_t *handler_exec,
		  void *handle, const char **error_msg, l4_addr_t *entry);

EXTERN_C_END

#endif

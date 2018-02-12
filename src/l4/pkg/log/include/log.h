/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU Lesser General Public License 2.1.
 * Please see the COPYING-LGPL-2.1 file for details.
 */
#pragma once

#include <l4/sys/compiler.h>
#include <stdarg.h>

EXTERN_C_BEGIN

L4_CV void LOG_printf(const char *format, ...)
  __attribute__((format(printf, 1, 2)));
L4_CV void LOG_vprintf(const char *format, va_list list);
L4_CV void LOG_log(const char *function, const char *format, ...)
  __attribute__((format(printf, 2, 3)));
L4_CV void LOG_logl(const char *file, int line, const char *function,
                    const char *format, ...)
  __attribute__((format(printf, 4, 5)));
L4_CV void LOG_flush(void);

EXTERN_C_END

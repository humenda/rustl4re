/*
 * \brief   Interface of DOpE terminal library
 * \date    2003-03-06
 * \author  Norman Feske <nf2@inf.tu-dresden.de>
 */

/*
 * Copyright (C) 2002-2003  Norman Feske  <nf2@os.inf.tu-dresden.de>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the DOpE package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */

#include <l4/sys/compiler.h>
#include <stdarg.h>

L4_CV int  term_init    (char *terminal_name);
L4_CV void term_deinit  (void);
L4_CV int  term_printf  (const char *format, ...);
L4_CV int  term_vprintf (const char *format, va_list ap);
L4_CV int  term_getchar (void);

struct history;

L4_CV struct history *term_history_create(void *buf, long buf_size);
L4_CV int   term_history_add(struct history *hist, char *new_str);
L4_CV char *term_history_get(struct history *hist, int index);

L4_CV int term_readline(char *dst, int max_len, struct history *hist);


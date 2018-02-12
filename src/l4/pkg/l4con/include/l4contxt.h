/*!
 * \file	l4con/include/l4contxt.h
 * \brief	libcontxt client interface
 *
 * \date	2002
 * \author	Mathias Noack <mn3@os.inf.tu-dresden.de> */
/*
 * (c) 2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#ifndef _L4CONTXT_L4CONTXT_H
#define _L4CONTXT_L4CONTXT_H

/* common interface include */
#include <l4/l4con/l4contxt_common.h>

/** \defgroup contxt_if Client Library for Text Output */

/** Init of contxt library.
 * \ingroup contxt_if
 *
 * \param   max_sbuf_size  ... max IPC string buffer
 * \param   scrbuf_lines   ... number of additional screenbuffer lines
 *
 * This is the init-function of libcontxt. It opens a console and allocates
 * the screen history buffer using malloc(). This functions must be called
 * before the other functions for text output may be used. */
L4_CV int contxt_init(long max_sbuf_size, int scrbuf_lines);

/** Close contxt library.
 * \ingroup contxt_if
 *
 * \return  0 on success (close a console)
 *          PANIC otherwise
 *
 * Close the libcontxt console. */
L4_CV int contxt_close(void);

#endif


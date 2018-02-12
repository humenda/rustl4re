/**
 * \file   input/lib/src/emul_res.c
 * \brief  L4INPUT: Linux I/O resource management emulation
 *
 * \author Christian Helmuth <ch12@os.inf.tu-dresden.de>
 *
 * Currently we only support I/O port requests.
 */
/*
 * (c) 2007-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/io/io.h>

void * request_region(unsigned long start, unsigned long len, const char *name)
{
	int err = l4io_request_ioport(start, len);
	return err ? 0 : (void *)1;
}

void release_region(unsigned long start, unsigned long len)
{
	l4io_release_ioport(start, len);
}


/**
 * \file   ferret/examples/merge_mon/uu.h
 * \brief  UUencoding functions, mostly from jdb
 *
 * \date   14/12/2005
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2005-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#ifndef __FERRET_EXAMPLES_MERGE_MON_UU_H_
#define __FERRET_EXAMPLES_MERGE_MON_UU_H_

#include <stddef.h>

//void uu_dump(const char * filename, const buf_t *buf);
void uu_dumpz(const char * filename, const unsigned char * s_buf, size_t len);
void uu_dumpz_ringbuffer(const char * filename,
                         const void * buffer, size_t buffer_len,
                         off_t start_offset, size_t transfer_size);

#endif

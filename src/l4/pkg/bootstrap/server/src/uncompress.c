/**
 * \file	bootstrap/server/src/uncompress.c
 * \brief	Support for on-the-fly uncompressing of boot modules
 * 
 * \date	2004
 * \author	Adam Lackorzynski <adam@os.inf.tu-dresden.de> */

/*
 * (c) 2005-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <l4/sys/l4int.h>
#include <l4/sys/consts.h>

#include "startup.h"
#include "gunzip.h"
#include "uncompress.h"

static void *filestart;

enum { lin_alloc_buffer_size = 32 << 10 };
unsigned long lin_alloc_buffer[(lin_alloc_buffer_size + sizeof(unsigned long) - 1)/ sizeof(unsigned long)];

/*
 * Upper address for the allocator for gunzip
 */
l4_addr_t
gunzip_upper_mem_linalloc(void)
{
  return (l4_addr_t)lin_alloc_buffer + sizeof(lin_alloc_buffer);
}

/*
 * Returns true if file is compressed, false if not
 */
static void
file_open(void *start,  int size)
{
  filepos = 0;
  filestart = start;
  filemax = size;
  fsmax = 0xffffffff; /* just big */
  compressed_file = 0;

  gunzip_test_header();
}

static int
module_read(void *buf, int size)
{
  memcpy(buf, (const void *)((l4_addr_t)filestart + filepos), size);
  filepos += size;
  return size;
}

int
grub_read(unsigned char *buf, int len)
{
  /* Make sure "filepos" is a sane value */
  if (/*(filepos < 0) || */(filepos > filemax))
    filepos = filemax;

  /* Make sure "len" is a sane value */
  if ((len < 0) || (len > (signed)(filemax - filepos)))
    len = filemax - filepos;

  /* if target file position is past the end of
     the supported/configured filesize, then
     there is an error */
  if (filepos + len > fsmax)
    {
      printf("Filesize error %ld + %d > %ld\n", filepos, len, fsmax);
      return 0;
    }

  if (compressed_file)
    return gunzip_read (buf, len);

  return module_read(buf, len);
}

void*
decompress(const char *name, void *start, void *destbuf,
           int size, int size_uncompressed)
{
  int read_size;

  if (!size_uncompressed)
    return NULL;

  file_open(start, size);

  // don't move data around if the data isn't compressed
  if (!compressed_file)
    return start;

  printf("  Uncompressing %s from %p to %p (%d to %d bytes, %+lld%%).\n",
        name, start, destbuf, size, size_uncompressed,
	100*(unsigned long long)size_uncompressed/size - 100);

  // Add 10 to detect too short given size
  if ((read_size = grub_read(destbuf, size_uncompressed + 10))
      != size_uncompressed)
    {
      printf("Uncorrect decompression: should be %d bytes but got %d bytes. Errnum = %d\n",
             size_uncompressed, read_size, errnum);
      return NULL;
    }

  return destbuf;
}

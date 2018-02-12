/*
 * (c) 2004-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU Lesser General Public License 2.1.
 * Please see the COPYING-LGPL-2.1 file for details.
 */

#include <l4/lxfuxlibc/lxfuxlc.h>

#define LX_FILE_BUFFERS 10
LX_FILE lx_file_buffer[LX_FILE_BUFFERS];

/* from dietlibc */
static int stdio_parse_mode(const char *mode)
{
  int m = 0;
  while (1) {
    switch (*mode) {
      case 0: return m;
      case 'b': break;
      case 'r': m = LX_O_RDONLY; break;
      case 'w': m = LX_O_WRONLY|LX_O_CREAT|LX_O_TRUNC; break;
      case 'a': m = LX_O_WRONLY|LX_O_CREAT|LX_O_APPEND; break;
      case '+': m = (m & (~LX_O_WRONLY)) | LX_O_RDWR; break;
    }
    mode++;
  }
}

static void simple_panic(void)
{
  *(int *)NULL = 123;
}

static LX_FILE *reserve_lx_file_buffer(void)
{
  unsigned int i;

  for (i = 0; i < LX_FILE_BUFFERS; i++) {
    if (!lx_file_buffer[i].taken) {
      lx_file_buffer[i].taken = 1;
      return &lx_file_buffer[i];
    }
  }
  simple_panic();
  return NULL;
}

static void free_lx_file_buffer(LX_FILE *stream)
{
  if (stream < lx_file_buffer || stream > lx_file_buffer + LX_FILE_BUFFERS)
    simple_panic();

  stream->taken = stream->fd = 0;
}

LX_FILE *lx_fopen(const char *filename, const char *mode)
{
  int m = stdio_parse_mode(mode);
  int fd;
  LX_FILE *f;

  if ((fd = lx_open(filename, m, 0666)) < 0)
    return NULL;
  if ((f = reserve_lx_file_buffer()) == NULL) {
    lx_close(fd);
    return NULL;
  }
  f->fd = fd;
  f->flags = 0;
  return f;
}

LX_FILE *lx_fdopen(int fildes, const char *mode)
{
  LX_FILE *f;

  (void)mode;
  if ((f = reserve_lx_file_buffer()) == NULL)
    return NULL;
  f->fd = fildes;
  return f;
}

lx_size_t lx_fread(void *ptr, lx_size_t size, lx_size_t nmemb, LX_FILE *f)
{
  int r = lx_read(f->fd, ptr, size * nmemb);
  if (r < 0)
    return 0;
  return r / size;
}

lx_size_t lx_fwrite(const void *ptr, lx_size_t size, lx_size_t nmemb, LX_FILE *f)
{
  int r = lx_write(f->fd, ptr, size * nmemb);
  if (r < 0)
    return 0;
  return r / size;
}

int lx_fseek(LX_FILE *f, long offset, int whence)
{
  return lx_lseek(f->fd, offset, whence) < 0 ? -1 : 0;
}

long lx_ftell(LX_FILE *f)
{
  return lx_lseek(f->fd, 0, SEEK_CUR);
}

void lx_rewind(LX_FILE *f)
{
  (void)lx_fseek(f, 0L, SEEK_SET);
}

int lx_fflush(LX_FILE *f)
{
  (void)f;
  /* We don't buffer (in userspace) */
  return 0;
}

int lx_fputc(int c, LX_FILE *f)
{
  int r = lx_write(f->fd, (char *)&c, 1);
  if (r == 1)
    return c;
  return LX_EOF;
}

int lx_fprintf(LX_FILE *f, const char *format, ...)
{
  (void)f; (void)format;
  lx_write(1, "lx_fprintf called\n", 19);
  return 0;
}

int lx_fclose(LX_FILE *f)
{
  int i = lx_close(f->fd);
  if (i == 0)
    free_lx_file_buffer(f);
  return i;
}

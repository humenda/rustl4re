#ifndef __BOOTSTRAP__GUNZIP_H__
#define __BOOTSTRAP__GUNZIP_H__

#include <l4/sys/l4int.h>
#include "panic.h"

typedef enum
{
  ERR_NONE = 0,
  ERR_BAD_GZIP_DATA,
  ERR_BAD_GZIP_HEADER,

} grub_error_t;

l4_addr_t gunzip_upper_mem_linalloc(void);
#define UPPER_MEM_LINALLOC gunzip_upper_mem_linalloc()

#define RAW_ADDR(x) (x)
#define RAW_SEG(x) (x)

extern unsigned long filepos;
extern unsigned long filemax;
extern unsigned long fsmax;     /* max size of fs/readable media */
extern unsigned int compressed_file;
extern grub_error_t errnum;

/* Read LEN bytes into BUF from the file that was opened with
   GRUB_OPEN.  If LEN is -1, read all the remaining data in the file.  */
int grub_read (unsigned char *buf, int len);

int gunzip_read (unsigned char *buf, int len);
int gunzip_test_header (void);

void *memmove(void *dest, const void *src, size_t n);

#endif /* ! __BOOTSTRAP__GUNZIP_H__ */

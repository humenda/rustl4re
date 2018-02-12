#include <stdio.h>
#include <string.h>
#include "libc_backend.h"

size_t strlen(const char *s)
{
  size_t l = 0;
  while (*s)
    {
      l++;
      s++;
    }
  return l;
}


int puts(const char *c)
{
  __libc_backend_outs(c, strlen(c));
  return __libc_backend_outs("\n", 1);
}

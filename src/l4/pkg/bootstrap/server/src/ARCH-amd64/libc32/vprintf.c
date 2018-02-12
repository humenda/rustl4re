#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#include "vprintf_backend.h"
#include "libc_backend.h"

static int
libc_backend_outs_wrapper(const char *s, size_t len, void *dummy)
{
  (void)dummy;
  return __libc_backend_outs(s, len);
}

int vprintf(const char *format, va_list ap)
{
  struct output_op _ap = { 0, &libc_backend_outs_wrapper };
  return __v_printf(&_ap,format,ap);
}

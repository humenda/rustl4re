/*!
 * \file	con/examples/xf86_stub/helper.c
 * \brief
 *
 * \date	01/2002
 * \author	Frank Mehnert <fm3@os.inf.tu-dresden.de>
 */

/*
 * Copyright (c) 2003 by Technische Universität Dresden, Germany
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * TECHNISCHE UNIVERSITÄT DRESDEN BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */

#include <stdarg.h>
#include <stddef.h>
//#include <stdio.h>
#include <l4/sys/types.h>
#include <l4/log/l4log.h>
#include <l4/env/env.h>
#include <xf86.h>

#undef size_t
#undef strcpy
#undef strncpy
#undef strlen
#undef printf

//typedef unsigned int size_t;

char* strcpy(char *to, const char *from);
char* strncpy(char *to, const char *from, size_t count);
size_t strlen(const char * s);

void
LOG_flush(void)
{
}

void
LOG_log(const char *function, const char *format, ...)
{
}


int printf(const char *format, ...);
int
printf(const char *format, ...)
{
  return 0;
}

#if 0
int
printf(const char *format, ...)
{
  va_list list;
  va_start(list, format);
  xf86DrvMsg(0, 5 /*X_ERROR*/, format, list);
  va_end(list);
  return 0;
}
#endif

char*
strncpy(char *to, const char *from, size_t count)
{
  register char *ret = to;

  while (count > 0) 
    {
      count--;
      if ((*to++ = *from++) == '\0')
	break;
    }

  while (count > 0) 
    {
      count--;
      *to++ = '\0';
    }
  
  return ret;
}

char*
strcpy(char *to, const char *from)
{
  register char *ret = to;

  while ((*to++ = *from++) != 0);

  return ret;
}

size_t
strlen(const char *s)
{
  int i;
  if (!s)
    return 0;
  for (i=0; *s; s++)
    i++;
  return i;
}

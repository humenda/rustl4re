/**
 * \file	bootstrap/server/src/libc_support.c
 * \brief	Support for C library
 *
 * \date	2004-2008
 * \author	Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *		Frank Mehnert <fm3@os.inf.tu-dresden.de> */

/*
 * (c) 2005-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/types.h>
#include "panic.h"

#include <l4/cxx/basic_ostream>

#include "support.h"

Platform_base *Platform_base::platform;

static L4::Uart *stdio_uart;

L4::Uart *uart()
{ return stdio_uart; }

void set_stdio_uart(L4::Uart *uart)
{ stdio_uart = uart; }


inline void *operator new (size_t, void *p) { return p; }
// IO Stream backend
namespace {

  class BootstrapIOBackend : public L4::IOBackend
  {
  protected:
    void write(char const *str, unsigned len);
  };

  void BootstrapIOBackend::write(char const *str, unsigned len)
  {
    ::write(STDOUT_FILENO, str, len);
  }

};

namespace L4 {
  typedef char Fake_iobackend[sizeof(BootstrapIOBackend)]
    __attribute__((aligned(__alignof__(BootstrapIOBackend))));
  typedef char Fake_ostream[sizeof(BasicOStream)]
    __attribute__((aligned(__alignof__(BasicOStream))));

  Fake_ostream cout;
  Fake_ostream cerr;

  static Fake_iobackend _iob;

  void iostream_init();
  void iostream_init()
  {
    static int _initialized;
    if (!_initialized)
      {
	_initialized = 1;
	BootstrapIOBackend *iob = new (&_iob) BootstrapIOBackend();
	new (&cerr) BasicOStream(iob);
	new (&cout) BasicOStream(iob);
      }
  }
};

typedef void Ctor();

static void call_ctors(Ctor **start, Ctor **end)
{
  for (; start < end; ++start)
    if (*start)
      (*start)();
}

void
ctor_init()
{
  extern Ctor *__CTORS_BEGIN[];
  extern Ctor *__CTORS_END[];
  extern Ctor *__init_array_start[];
  extern Ctor *__init_array_end[];
  extern Ctor *__preinit_array_start[];
  extern Ctor *__preinit_array_end[];

  call_ctors(__preinit_array_start, __preinit_array_end);
  call_ctors(__CTORS_BEGIN, __CTORS_END);
  call_ctors(__init_array_start, __init_array_end);
}


void exit(int c) throw()
{
  _exit(c);
}

void (*__exit_cleanup) (int) = 0;

extern "C" void __attribute__((noreturn))
__assert(const char *, const char *, int, register const char *);

extern "C" void __attribute__((noreturn))
__assert(const char *assertion, const char * filename,
         int linenumber, register const char * function)
{
  printf("%s:%d: %s: Assertion `%s' failed.\n",
				filename,
				linenumber,
				((function == NULL) ? "?function?" : function),
				assertion
				);
  panic("Assertion");
  while(1)
    ;
}

ssize_t
write(int fd, const void *buf, size_t count)
{
  if (!uart())
    return count;

  if (fd == STDOUT_FILENO || fd == STDERR_FILENO)
    {
      char *b = (char *)buf;
      int i = count;
      while (i--)
        {
          char c = *b++;
          if (c == '\n')
            uart()->write("\r", 1);
          uart()->write(&c, 1);
        }

      return count;
    }

  errno = EBADF;
  return -1;
}

#undef getchar
int
getchar(void)
{
  int c;
  if (!uart())
    return -1;

  do
    c = uart()->get_char(0);
  while (c == -1);
  return c;
}

off_t lseek(int /*fd*/, off_t /*offset*/, int /*whence*/)
{
  return 0;
}

void *__dso_handle = &__dso_handle;

extern "C" void reboot(void) __attribute__((noreturn));
void reboot(void)
{
  void reboot_arch() __attribute__((noreturn));
  reboot_arch();
}

extern "C" void __attribute__((noreturn))
_exit(int /*rc*/)
{
  printf("\n\033[1mKey press reboots...\033[m\n");
  getchar();
  printf("Rebooting.\n\n");
  reboot();
}

/** for assert */
void
abort(void) throw()
{
  _exit(1);
}

void
panic(const char *fmt, ...)
{
  va_list v;
  va_start (v, fmt);
  vprintf(fmt, v);
  va_end(v);
  putchar('\n');
  exit(1);
}

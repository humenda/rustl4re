/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>,
 *               Frank Mehnert <fm3@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <pthread-l4.h>
#include <l4/util/util.h>
#include <l4/sys/kdebug.h>
#if defined(ARCH_x86) || defined(ARCH_amd64)
#include <l4/util/rdtsc.h>
#endif
#include <l4/re/env.h>
#include <pthread.h>

#include <l4/l4re_vfs/backend>

#if !defined(ARCH_x86) && !defined(ARCH_amd64)
static void l4_busy_wait_ns(unsigned long val)
{
  static volatile int fooo = 0;
  unsigned long i;
  for (i = val/10; i > 0; --i)
    ++fooo;
}
#endif


static void *
thread(void *id)
{
  printf("\033[31merrno_location[%ld] = %08lx\033[m\n", 
      (unsigned long)id, (l4_addr_t)__errno_location());
  for (;;)
    {
      if (id == (void*)1)
	puts("abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz");
      else
	puts("ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZ");
      l4_busy_wait_ns(1000000);
    }

  return NULL;
}

namespace {

using namespace L4Re::Vfs;

class myops : public Be_file
{
public:
  ssize_t writev(const struct iovec *i, int) throw();
  int fstat64(struct stat64 *buf) const throw()
  { (void)buf; return 0; }
};

ssize_t
myops::writev(const struct iovec *i, int) throw()
{
  const char *b = (char *)i->iov_base;
  size_t      c = i->iov_len;

  while (c--)
    {
      outchar(*b++);
      l4_busy_wait_ns(1000000);
    }
  return i->iov_len;
}

static void f() __attribute__((constructor));
static void f()
{
  static myops mo;
  mo.add_ref();
  L4Re::Vfs::vfs_ops->set_fd(STDOUT_FILENO, cxx::ref_ptr(&mo));
}
}

int
main(void)
{
#if defined(ARCH_x86) || defined(ARCH_amd64)
  l4_calibrate_tsc(l4re_kip());
#endif
  dup2(STDOUT_FILENO, STDERR_FILENO);
  pthread_t t1, t2;
  pthread_create(&t1, NULL, thread, (void *)1);
  pthread_create(&t2, NULL, thread, (void *)2);
  l4_sleep_forever();
}

/*!
 * \file   ux.c
 * \brief  open/read/seek/close for UX
 *
 * \date   2008-02-27
 * \author Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *
 */
/*
 * (c) 2008-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU Lesser General Public License 2.1.
 * Please see the COPYING-LGPL-2.1 file for details.
 */

#include <l4/lxfuxlibc/lxfuxlc.h>
#include <l4/l4re_vfs/backend>
#include <l4/util/util.h>

#include <cstdlib>
#include <cstring>
#include <cassert>

//#include <cstdio> // for debug printf

#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>

#include <l4/sys/thread.h>
#include <l4/re/env.h>
namespace {

using namespace L4Re::Vfs;
using cxx::Ref_ptr;

class Ux_dir : public Be_file
{
public:
  explicit Ux_dir(int fd = -1) throw() : Be_file(), _fd(fd) {}
  int get_entry(const char *, int, mode_t, Ref_ptr<File> *) throw();
  int fstat64(struct stat64 *buf) const throw();
  int mkdir(const char *, mode_t) throw();
  int unlink(const char *) throw();
  int rename(const char *, const char *) throw();
  int faccessat(const char *path, int mode, int flags) throw();
  ssize_t getdents(char *buf, size_t nbytes) throw();
  ~Ux_dir() throw() { if (_fd >= 0) lx_close(_fd); }

private:
  int _fd;
};


class Ux_file : public Be_file
{
public:
  explicit Ux_file(int fd = -1) throw() : Be_file(), _fd(fd) {}

  ssize_t readv(const struct iovec*, int iovcnt) throw();
  ssize_t writev(const struct iovec*, int iovcnt) throw();
  ssize_t preadv(const struct iovec *iov, int iovcnt, off64_t offset) throw();
  ssize_t pwritev(const struct iovec *iov, int iovcnt, off64_t offset) throw();

  off64_t lseek64(off64_t, int) throw();
  int fstat64(struct stat64 *buf) const throw();

  //int fcntl64(int cmd, unsigned long arg);
  //int ioctl(unsigned long request, void *argp);
  //int close();

  ~Ux_file() throw() { if (_fd >= 0) lx_close(_fd); }

  //void fd(int f) { _fd = f; }

private:
  int _fd;
};


int
Ux_dir::get_entry(const char *name, int flags, mode_t mode,
                  Ref_ptr<File> *file) throw()
{
  //printf("get_entry: '%s'\n", name);
  if (!*name)
    name = ".";
  l4_touch_ro(name, strlen(name));
  int fd = lx_openat(_fd, name, flags, mode);
  if (fd < 0)
    return fd;

  struct stat64 sb;

  int res = lx_fstat64(fd, (struct lx_stat64 *)&sb);
  if (res < 0)
    return res;

  if (S_ISDIR(sb.st_mode))
    {
      //printf("open '%s' as directory\n", name);
      *file = cxx::ref_ptr(new Ux_dir(fd));
    }
  else
    {
      //printf("open '%s' as file\n", name);
      *file = cxx::ref_ptr(new Ux_file(fd));
    }

  if (!*file)
    return -ENOMEM;

  return 0;
}

int
Ux_dir::fstat64(struct stat64 *buf) const throw()
{
  return lx_fstat64(_fd, (struct lx_stat64*)buf);
}

int
Ux_dir::faccessat(const char *path, int mode, int flags) throw()
{
  (void)flags;
  if (*path == 0)
    path = ".";
  l4_touch_ro(path, strlen(path));
  return lx_faccessat(_fd, path, mode);
}

int
Ux_dir::mkdir(const char *name, mode_t mode) throw()
{
  l4_touch_ro(name, strlen(name));
  return lx_mkdirat(_fd, name, mode);
}

int
Ux_dir::unlink(const char *name) throw()
{
  l4_touch_ro(name, strlen(name));
  return lx_unlinkat(_fd, name, 0);
}

int
Ux_dir::rename(const char *old, const char *newn) throw()
{
  l4_touch_ro(old, strlen(old));
  l4_touch_ro(newn, strlen(newn));
  return lx_renameat(_fd, old, _fd, newn);
}

ssize_t
Ux_dir::getdents(char *buf, size_t nbytes) throw()
{
#define kernel_dirent64 lx_linux_dirent64
#define     MIN(a,b) (((a)<(b))?(a):(b))
  // taken from uclibc/ARCH-all/libc/sysdeps/linux/common/getdents64.c
    struct dirent64 *dp;
    off64_t last_offset = -1;
    ssize_t retval;
    size_t red_nbytes;
    struct kernel_dirent64 *skdp, *kdp;
    const size_t size_diff = (offsetof (struct dirent64, d_name)
            - offsetof (struct kernel_dirent64, d_name));

    red_nbytes = MIN (nbytes - ((nbytes /
                    (offsetof (struct dirent64, d_name) + 14)) * size_diff),
            nbytes - size_diff);

    dp = (struct dirent64 *) buf;
    skdp = kdp = (lx_linux_dirent64*)alloca (red_nbytes);

    l4_touch_rw(kdp, red_nbytes);

    retval = lx_getdents64(_fd, (char *)kdp, red_nbytes);
    if (retval < 0)
        return retval;

    while ((char *) kdp < (char *) skdp + retval) {
        const size_t alignment = __alignof__ (struct dirent64);
        /* Since kdp->d_reclen is already aligned for the kernel structure
           this may compute a value that is bigger than necessary.  */
        size_t new_reclen = ((kdp->d_reclen + size_diff + alignment - 1)
                & ~(alignment - 1));
        if ((char *) dp + new_reclen > buf + nbytes) {
            /* Our heuristic failed.  We read too many entries.  Reset
               the stream.  */
            assert (last_offset != -1);

            //lx_lseek64(_fd, last_offset, SEEK_SET);
            lx_lseek(_fd, last_offset, SEEK_SET);

            if ((char *) dp == buf) {
                /* The buffer the user passed in is too small to hold even
                   one entry.  */
                errno = EINVAL;
                return -1;
            }
            break;
        }

        last_offset = kdp->d_off;
        dp->d_ino = kdp->d_ino;
        dp->d_off = kdp->d_off;
        dp->d_reclen = new_reclen;
        dp->d_type = kdp->d_type;
        memcpy (dp->d_name, kdp->d_name,
                kdp->d_reclen - offsetof (struct kernel_dirent64, d_name));
        dp = (struct dirent64 *) ((char *) dp + new_reclen);
        kdp = (struct kernel_dirent64 *) (((char *) kdp) + kdp->d_reclen);
    }
    return (char *) dp - buf;
#undef MIN
#undef kernel_dirent64
}

/* Files */

ssize_t
Ux_file::readv(const struct iovec *iov, int cnt) throw()
{
  for (int i = 0; i < cnt; ++i)
    l4_touch_rw(iov[i].iov_base, iov[i].iov_len);

  return lx_readv(_fd, (struct lx_iovec*)iov, cnt);
}

ssize_t
Ux_file::writev(const struct iovec *iov, int cnt) throw()
{
  for (int i = 0; i < cnt; ++i)
    l4_touch_ro(iov[i].iov_base, iov[i].iov_len);

  return lx_writev(_fd, (struct lx_iovec*)iov, cnt);
}

ssize_t
Ux_file::preadv(const struct iovec *iov, int iovcnt, off64_t offset) throw()
{
  for (int i = 0; i < iovcnt; ++i)
    l4_touch_rw(iov[i].iov_base, iov[i].iov_len);

  return lx_preadv(_fd, (struct lx_iovec*)iov, iovcnt, offset, offset << 32);
}

ssize_t
Ux_file::pwritev(const struct iovec *iov, int iovcnt, off64_t offset) throw()
{
  for (int i = 0; i < iovcnt; ++i)
    l4_touch_ro(iov[i].iov_base, iov[i].iov_len);

  return lx_pwritev(_fd, (struct lx_iovec*)iov, iovcnt, offset, offset << 32);
}

off64_t
Ux_file::lseek64(off64_t offset, int whence) L4_NOTHROW
{
  //printf("lseek: fd=%d offset=%lld\n", _fd, offset);
  return lx_lseek(_fd, offset, whence);
}

#if 0
int
ux_file_ops::fcntl64(int cmd, unsigned long arg)
{
  return lx_fcntl64(_fd, cmd, arg);
}
#endif

int
Ux_file::fstat64(struct stat64 *buf) const throw()
{
  l4_touch_rw(buf, sizeof(*buf));
  return lx_fstat64(_fd, (struct lx_stat64 *)buf);
}

#if 0
int
ux_file_ops::ioctl(unsigned long cmd, void *argp)
{
  return lx_ioctl(_fd, cmd, (unsigned long)argp);
}
#endif

class Ux_fs : public Be_file_system
{
public:
  Ux_fs() : Be_file_system("fuxfs")
  {
    l4_thread_control_start();
    l4_thread_control_ux_host_syscall(1);
    l4_thread_control_commit(l4re_env()->main_thread);
  }

  int mount(char const *source, unsigned long mountflags,
            void const *data, cxx::Ref_ptr<File> *dir) throw()
  {
    (void)mountflags;
    (void)data;

    l4_touch_ro(source, strlen(source));
    int fd = lx_open(source, O_DIRECTORY, 0);
    if (fd < 0)
      return fd;

    *dir = cxx::ref_ptr(new Ux_dir(fd));
    if (!*dir)
      return -ENOMEM;
    return 0;
  }
};

static Ux_fs _ux_fs L4RE_VFS_FILE_SYSTEM_ATTRIBUTE;

}

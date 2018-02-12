/**
 */
/*
 * (c) 2010 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *          Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU Lesser General Public License 2.1.
 * Please see the COPYING-LGPL-2.1 file for details.
 */

#include <l4/l4re_vfs/backend>
#include <l4/cxx/string>
#include <l4/cxx/avl_tree>

#include <sys/stat.h>
#include <sys/ioctl.h>
#include <dirent.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

using namespace L4Re::Vfs;
using cxx::Ref_ptr;

class File_data
{
public:
  File_data() : _buf(0), _size(0) {}

  unsigned long put(unsigned long offset,
                    unsigned long bufsize, void *srcbuf);
  unsigned long get(unsigned long offset,
                    unsigned long bufsize, void *dstbuf);

  unsigned long size(unsigned long offset);
  unsigned long size() const { return _size; }

  ~File_data() throw() { free(_buf); }

private:
  void *_buf;
  unsigned long _size;
};

unsigned long
File_data::put(unsigned long offset, unsigned long bufsize, void *srcbuf)
{
  if (offset + bufsize > _size)
    size(offset + bufsize);

  if (!_buf)
    return 0;

  memcpy((char *)_buf + offset, srcbuf, bufsize);
  return bufsize;
}

unsigned long
File_data::get(unsigned long offset, unsigned long bufsize, void *dstbuf)
{
  unsigned long s = bufsize;

  if (offset > _size)
    return 0;

  if (offset + bufsize > _size)
    s = _size - offset;

  memcpy(dstbuf, (char *)_buf + offset, s);
  return s;
}

unsigned long
File_data::size(unsigned long offset)
{
  if (offset != _size)
    {
      _size = offset;
      _buf = realloc(_buf, _size);
    }

  if (_buf)
    return 0;
  return -ENOSPC;
}


class Node : public cxx::Avl_tree_node
{
public:
  Node(const char *path, mode_t mode)
    : _ref_cnt(0), _path(strdup(path))
  {
    memset(&_info, 0, sizeof(_info));
    _info.st_mode = mode;
  }

  const char *path() const { return _path; }
  struct stat64 *info() { return &_info; }

  void add_ref() throw() { ++_ref_cnt; }
  int remove_ref() throw() { return --_ref_cnt; }

  bool is_dir() const { return S_ISDIR(_info.st_mode); }

  virtual ~Node() { free(_path); }

private:
  int           _ref_cnt;
  char         *_path;
  struct stat64 _info;
};

struct Node_get_key
{
  typedef cxx::String Key_type;
  static Key_type key_of(Node const *n)
  { return n->path(); }
};

struct Path_avl_tree_compare
{
  bool operator () (const char *l, const char *r) const
  { return strcmp(l, r) < 0; }
  bool operator () (const cxx::String l, const cxx::String r) const
  {
    int v = strncmp(l.start(), r.start(), cxx::min(l.len(), r.len()));
    return v < 0 || (v == 0 && l.len() < r.len());
  }
};

class Pers_file : public Node
{
public:
  Pers_file(const char *name, mode_t mode)
    : Node(name, (mode & 0777) | __S_IFREG) {}
  File_data const &data() const { return _data; }
  File_data &data() { return _data; }
private:
  File_data     _data;
};

class Pers_dir : public Node
{
private:
  typedef cxx::Avl_tree<Node, Node_get_key, Path_avl_tree_compare> Tree;
  Tree _tree;

public:
  Pers_dir(const char *name, mode_t mode)
    : Node(name, (mode & 0777) | __S_IFDIR) {}
  Ref_ptr<Node> find_path(cxx::String);
  bool add_node(Ref_ptr<Node> const &);

  typedef Tree::Const_iterator Const_iterator;
  Const_iterator begin() const { return _tree.begin(); }
  Const_iterator end() const { return _tree.end(); }
};

Ref_ptr<Node> Pers_dir::find_path(cxx::String path)
{
  return cxx::ref_ptr(_tree.find_node(path));
}

bool Pers_dir::add_node(Ref_ptr<Node> const &n)
{
  bool e = _tree.insert(n.ptr()).second;
  if (e)
    n->add_ref();
  return e;
}

class Tmpfs_dir : public Be_file
{
public:
  explicit Tmpfs_dir(Ref_ptr<Pers_dir> const &d) throw()
    : _dir(d), _getdents_state(false) {}
  int get_entry(const char *, int, mode_t, Ref_ptr<File> *) throw();
  ssize_t getdents(char *, size_t) throw();
  int fstat64(struct stat64 *buf) const throw();
  int utime(const struct utimbuf *) throw();
  int fchmod(mode_t) throw();
  int mkdir(const char *, mode_t) throw();
  int unlink(const char *) throw();
  int rename(const char *, const char *) throw();
  int faccessat(const char *, int, int) throw();

private:
  int walk_path(cxx::String const &_s,
                Ref_ptr<Node> *ret, cxx::String *remaining = 0);

  Ref_ptr<Pers_dir> _dir;
  bool _getdents_state;
  Pers_dir::Const_iterator _getdents_iter;
};

class Tmpfs_file : public Be_file_pos
{
public:
  explicit Tmpfs_file(Ref_ptr<Pers_file> const &f) throw()
    : Be_file_pos(), _file(f) {}

  off64_t size() const throw();
  int fstat64(struct stat64 *buf) const throw();
  int ftruncate64(off64_t p) throw();
  int ioctl(unsigned long, va_list) throw();
  int utime(const struct utimbuf *) throw();
  int fchmod(mode_t) throw();

private:
  ssize_t preadv(const struct iovec *v, int iovcnt, off64_t p) throw();
  ssize_t pwritev(const struct iovec *v, int iovcnt, off64_t p) throw();
  Ref_ptr<Pers_file> _file;
};

ssize_t Tmpfs_file::preadv(const struct iovec *v, int iovcnt, off64_t p) throw()
{
  if (iovcnt < 0)
    return -EINVAL;

  ssize_t sum = 0;
  for (int i = 0; i < iovcnt; ++i)
    {
      sum  += _file->data().get(p, v[i].iov_len, v[i].iov_base);
      p += v[i].iov_len;
    }
  return sum;
}

ssize_t Tmpfs_file::pwritev(const struct iovec *v, int iovcnt, off64_t p) throw()
{
  if (iovcnt < 0)
    return -EINVAL;

  ssize_t sum = 0;
  for (int i = 0; i < iovcnt; ++i)
    {
      sum  += _file->data().put(p, v[i].iov_len, v[i].iov_base);
      p += v[i].iov_len;
    }
  return sum;
}

int Tmpfs_file::fstat64(struct stat64 *buf) const throw()
{
  _file->info()->st_size = _file->data().size();
  memcpy(buf, _file->info(), sizeof(*buf));
  return 0;
}

int Tmpfs_file::ftruncate64(off64_t p) throw()
{
  if (p < 0)
      return -EINVAL;

  if (_file->data().size(p) == 0)
      return 0;

  return -EIO; // most likely ENOSPC, but can't report that
}

off64_t Tmpfs_file::size() const throw()
{ return _file->data().size(); }

int
Tmpfs_file::ioctl(unsigned long v, va_list args) throw()
{
  switch (v)
    {
    case FIONREAD: // return amount of data still available
      int *available = va_arg(args, int *);
      *available = _file->data().size() - pos();
      return 0;
    };
  return -EINVAL;
}

int
Tmpfs_file::utime(const struct utimbuf *times) throw()
{
  _file->info()->st_atime = times->actime;
  _file->info()->st_mtime = times->modtime;
  return 0;
}

int
Tmpfs_file::fchmod(mode_t m) throw()
{
  _file->info()->st_mode = m;
  return 0;
}

int
Tmpfs_dir::faccessat(const char *path, int mode, int) throw()
{
  Ref_ptr<Node> node;
  cxx::String name = path;

  int err = walk_path(name, &node, &name);
  if (err < 0)
    return err;

  if (mode == F_OK) // existence check
    return 0;

  struct stat64 *stats = node->info();

  if ((mode & R_OK) && !(stats->st_mode & S_IRUSR))
    return -EACCES;

  if ((mode & W_OK) && !(stats->st_mode & S_IWUSR))
    return -EACCES;

  if ((mode & X_OK) && !(stats->st_mode & S_IXUSR))
    return -EACCES;

  return 0;
}

int
Tmpfs_dir::get_entry(const char *name, int flags, mode_t mode,
                     Ref_ptr<File> *file) throw()
{
  Ref_ptr<Node> path;
  if (!*name)
    {
      *file = cxx::ref_ptr(this);
      return 0;
    }

  cxx::String n = name;

  int e = walk_path(n, &path, &n);

  if (e == -ENOTDIR)
    return e;

  if (!(flags & O_CREAT) && e < 0)
    return e;

  if ((flags & O_CREAT) && e == -ENOENT)
    {
      Ref_ptr<Node> node(new Pers_file(n.start(), mode));
      // when ENOENT is return, path is always a directory
      bool e = cxx::ref_ptr_static_cast<Pers_dir>(path)->add_node(node);
      if (!e)
        return -ENOMEM;
      path = node;
    }

  if (path->is_dir())
    *file = cxx::ref_ptr(new Tmpfs_dir(cxx::ref_ptr_static_cast<Pers_dir>(path)));
  else
    *file = cxx::ref_ptr(new Tmpfs_file(cxx::ref_ptr_static_cast<Pers_file>(path)));

  if (!*file)
    return -ENOMEM;


  return 0;
}

ssize_t
Tmpfs_dir::getdents(char *buf, size_t sz) throw()
{
  struct dirent64 *d = (struct dirent64 *)buf;
  ssize_t ret = 0;

  if (!_getdents_state)
    {
      _getdents_iter = _dir->begin();
      _getdents_state = true;
    }
  else if (_getdents_iter == _dir->end())
    {
      _getdents_state = false;
      return 0;
    }

  for (; _getdents_iter != _dir->end(); ++_getdents_iter)
    {
      unsigned l = strlen(_getdents_iter->path()) + 1;
      if (l > sizeof(d->d_name))
        l = sizeof(d->d_name);

      unsigned n = offsetof (struct dirent64, d_name) + l;
      n = (n + sizeof(long) - 1) & ~(sizeof(long) - 1);

      if (n > sz)
        break;

      d->d_ino = 1;
      d->d_off = 0;
      memcpy(d->d_name, _getdents_iter->path(), l);
      d->d_reclen = n;
      d->d_type   = DT_REG;
      ret += n;
      sz  -= n;
      d    = (struct dirent64 *)((unsigned long)d + n);
    }

  return ret;
}

int
Tmpfs_dir::fstat64(struct stat64 *buf) const throw()
{
  memcpy(buf, _dir->info(), sizeof(*buf));
  return 0;
}

int
Tmpfs_dir::utime(const struct utimbuf *times) throw()
{
  _dir->info()->st_atime = times->actime;
  _dir->info()->st_mtime = times->modtime;
  return 0;
}

int
Tmpfs_dir::fchmod(mode_t m) throw()
{
  _dir->info()->st_mode = m;
  return 0;
}

int
Tmpfs_dir::walk_path(cxx::String const &_s,
                     Ref_ptr<Node> *ret, cxx::String *remaining)
{
  Ref_ptr<Pers_dir> p = _dir;
  cxx::String s = _s;
  Ref_ptr<Node> n;

  while (1)
    {
      if (s.len() == 0)
        {
          *ret = p;
          return 0;
        }

      cxx::String::Index sep = s.find("/");

      if (sep - s.start() == 1 && *s.start() == '.')
        {
          s = s.substr(s.start() + 2);
          continue;
        }

      n = p->find_path(s.head(sep - s.start()));

      if (!n)
        {
          *ret = p;
          if (remaining)
            *remaining = s.head(sep - s.start());
          return -ENOENT;
        }


      if (sep == s.end())
        {
          *ret = n;
          return 0;
        }

      if (!n->is_dir())
        return -ENOTDIR;

      s = s.substr(sep + 1);

      p = cxx::ref_ptr_static_cast<Pers_dir>(n);
    }

  *ret = n;

  return 0;
}

int
Tmpfs_dir::mkdir(const char *name, mode_t mode) throw()
{
  Ref_ptr<Node> node = _dir;
  cxx::String p = cxx::String(name);
  cxx::String path, last = p;
  cxx::String::Index s = p.rfind("/");

  // trim /'s at the end
  while (p.len() && s == p.end() - 1)
    {
      p.len(p.len() - 1);
      s = p.rfind("/");
    }

  //printf("MKDIR '%s' p=%p %p\n", name, p.start(), s);

  if (s != p.end())
    {
      path = p.head(s);
      last = p.substr(s + 1, p.end() - s);

      int e = walk_path(path, &node);
      if (e < 0)
        return e;
    }

  if (!node->is_dir())
    return -ENOTDIR;

  // due to path walking we can end up with an empty name
  if (p.len() == 0 || p == cxx::String("."))
    return 0;

  Ref_ptr<Pers_dir> dnode = cxx::ref_ptr_static_cast<Pers_dir>(node);

  Ref_ptr<Pers_dir> dir(new Pers_dir(last.start(), mode));
  return dnode->add_node(dir) ? 0 : -EEXIST;
}

int
Tmpfs_dir::unlink(const char *name) throw()
{
  cxx::Ref_ptr<Node> n;

  int e = walk_path(name, &n);
  if (e < 0)
    return -ENOENT;

  printf("Unimplemented (if file exists): %s(%s)\n", __func__, name); 
  return -ENOMEM;
}

int
Tmpfs_dir::rename(const char *old, const char *newn) throw()
{
  printf("Unimplemented: %s(%s, %s)\n", __func__, old, newn); 
  return -ENOMEM;
}



class Tmpfs_fs : public Be_file_system
{
public:
  Tmpfs_fs() : Be_file_system("tmpfs") {}
  int mount(char const *source, unsigned long mountflags,
            void const *data, cxx::Ref_ptr<File> *dir) throw()
  {
    (void)mountflags;
    (void)source;
    (void)data;
    *dir = cxx::ref_ptr(new Tmpfs_dir(cxx::ref_ptr(new Pers_dir("root", 0777))));
    if (!*dir)
      return -ENOMEM;
    return 0;
  }
};

static Tmpfs_fs _tmpfs L4RE_VFS_FILE_SYSTEM_ATTRIBUTE;

}

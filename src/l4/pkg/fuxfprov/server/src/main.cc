/**
 * \file
 */
/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

/************************************************************************
 * This is an implementation made by wimps (actually only one).
 *
 * What we really need to do is to implement the dataspace protocol
 * and have a few pages buffer cache, and then also some ram pages for COW,
 * and some more feature-rich API...
 */


#include <l4/re/env>
#include <l4/re/namespace>
#include <l4/re/error_helper>
#include <l4/re/util/cap_alloc>
#include <l4/re/util/object_registry>
#include <l4/re/dataspace>
#include <l4/re/dataspace-sys.h>
#include <l4/re/namespace>
#include <l4/re/namespace-sys.h>
#include <l4/re/mem_alloc>
#include <l4/sys/factory>
#include <l4/cxx/ipc_server>
#include <l4/cxx/iostream>
#include <l4/cxx/l4iostream>
#include <l4/sys/thread.h>

#include <sys/mount.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <zlib.h>
#include <errno.h>

static const char *fprov_prefix = "ux";

static L4Re::Util::Registry_server<> server;

enum {
  Max_search_paths = 20,
};

static char *search_paths[Max_search_paths];
static int   nrpaths;

static void setup(int argc, char *argv[])
{
  if (argc <= 1)
    return;

  if (argc < 3 || strcmp(argv[1], "-p"))
    {
      fprintf(stderr, "Usage: %s [-p searchpath1:searchpath2:...]\n",
              argv[0]);
      throw (L4::Runtime_error(-L4_EINVAL));
    }

  char *p = argv[2];
  nrpaths = 0;

  do
    {
      while (*p == ':')
        {
          *p = 0;
          p++;
        }

      if (!*p)
        break;

      search_paths[nrpaths] = p;
      ++nrpaths;
      if (nrpaths == Max_search_paths)
        {
          fprintf(stderr, "Error, too many search paths, can only store %d\n",
                  Max_search_paths);
          throw (L4::Runtime_error(-L4_EINVAL));
        }
    } while ((p = strstr(p, ":")));
}


class Ds : public L4::Server_object_t<L4Re::Dataspace>
{
public:
  explicit Ds(const char *fname);
  int dispatch(l4_umword_t obj, L4::Ipc::Iostream &ios);

private:
  unsigned long flags() { return 0; }
  unsigned long _size;
  L4::Cap<L4Re::Dataspace> mem_ds;
  int map(l4_addr_t offs, l4_addr_t hot_spot, unsigned long flags,
          l4_addr_t min, l4_addr_t max, L4::Ipc::Snd_fpage &snd_fpage);
  unsigned evict_page();

  gzFile fd;
  unsigned long pos;
  void *mem_addr;
  int mem_count;
  enum {
    mem_pages = 1000,
  };
  struct mem_info {
    unsigned count;
  };
  struct mem_info mem_info[mem_pages];

  void *mem(unsigned idx)
    { return (char *)mem_addr + idx * L4_PAGESIZE; }
  void use_page(unsigned idx)
    { ++mem_count; mem_info[idx].count = mem_count; }

};

Ds::Ds(const char *fname)
  : mem_count(0)
{
  long fread;
  unsigned int fsize = 0;
  char buf[8 << 10];

  memset(mem_info, 0, sizeof(mem_info));

  printf("Ds-open: %s\n", fname);

  // use buf for filename buffer
  if (fname[0] == '/')
    {
      snprintf(buf, sizeof(buf), "/%s/%s", fprov_prefix, fname);
      buf[sizeof(buf) - 1] = 0;
      fd = gzopen(buf, "r");
    }
  else
    for (int i = 0; i < nrpaths; ++i)
      {
        snprintf(buf, sizeof(buf), "/%s/%s/%s", fprov_prefix, search_paths[i], fname);
        buf[sizeof(buf) - 1] = 0;
        printf("expanded: %s\n", buf);
        if ((fd = gzopen(buf, "r")))
          break;
      }

  if (fd == NULL)
    {
      printf("Can't open \"%s\": -%d\n", fname, errno);
      throw(L4::Element_not_found());
    }

  /* Get the size of the _uncompressed_ file */
  while (1)
    {
      if ((fread = gzread(fd, buf, sizeof(buf))) == -1)
        {
          printf("Error reading (or decoding) file %s: -%d\n", fname, errno);
          throw(L4::Runtime_error(-L4_EIO));
        }
      if (fread == 0)
        break;

      fsize += fread;
    }

  printf("size: %d\n", fsize);
  _size = fsize;


  gzseek(fd, 0, SEEK_SET);
  pos = 0;

  // get some pages
  mem_ds = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();
  if (!mem_ds.is_valid())
    {
      printf("Cap allocator failed\n");
      throw(L4::Out_of_memory());
    }

  int ret;
  if ((ret = L4Re::Env::env()->mem_alloc()->alloc(mem_pages * L4_PAGESIZE, mem_ds)))
    {
      printf("Can't allocate dataspace with size %ld (error %d)\n",
             _size, ret);
      L4Re::Util::cap_alloc.free(mem_ds, L4Re::This_task);
      gzclose(fd);
      throw(L4::Out_of_memory());
    }

  mem_addr = 0;
  if ((ret = L4Re::Env::env()->rm()->attach(&mem_addr, mem_pages * L4_PAGESIZE,
                                            L4Re::Rm::Search_addr,
                                            mem_ds, 0)))
    {
      printf("Attach failed with error %d\n", ret);
      L4Re::Util::cap_alloc.free(mem_ds, L4Re::This_task);
      gzclose(fd);
      throw(L4::Out_of_memory());
    }
  printf("temp memory is from %p - %lx\n",
         mem_addr, (unsigned long)mem_addr + mem_pages * L4_PAGESIZE);

  server.registry()->register_obj(this);
}

unsigned Ds::evict_page()
{
  // find the 'oldest page'
  unsigned c = ~0U, r = 0;

  for (unsigned i = 0; i < mem_pages; ++i)
    if (mem_info[i].count < c)
      {
        c = mem_info[i].count;
        r = i;
      }
  return r;
}

int Ds::map(l4_addr_t offs, l4_addr_t hot_spot, unsigned long flags,
            l4_addr_t, l4_addr_t, L4::Ipc::Snd_fpage &snd_fpage)
{
  printf("map: offs=%lx hot_spot=%lx flags=%lx\n", offs, hot_spot, flags);

  snd_fpage = L4::Ipc::Snd_fpage();

  if (offs > l4_round_page(_size))
    return -L4_ENOENT;

  offs = l4_trunc_page(offs);

  if (pos != offs)
    {
    //  printf("seek to %lx\n", offs);
      gzseek(fd, offs, SEEK_SET);
      pos = offs;
    }

  unsigned idx = evict_page();
  use_page(idx);

  // take away page in other address space
  L4Re::Env::env()->task()->unmap(l4_fpage((l4_addr_t)mem(idx), L4_PAGESHIFT, L4_FPAGE_RWX),
                                  L4_FP_OTHER_SPACES);

  long fread = 0, to_read = L4_PAGESIZE, o = 0;
  do
    {
      to_read -= fread;
      if ((fread = gzread(fd, (char *)mem(idx) + o, to_read)) == -1)
        return -L4_ENOENT;
   //   printf("f %ld (%d)\n", fread, gztell(fd));
      o += fread;
    }
  while (to_read - fread > 0 && fread > 0);

  //printf("read 0x%lx bytes from pos %d\n", o, pos);

  pos += o;

  memset((char *)mem(idx) + o, 0, L4_PAGESIZE - o);

  hot_spot = l4_trunc_page(hot_spot);

  snd_fpage = L4::Ipc::Snd_fpage(l4_fpage((l4_addr_t)mem(idx), L4_PAGESHIFT, L4_FPAGE_RO),
                            hot_spot);

  return -L4_EOK;
}

int Ds::dispatch(l4_umword_t obj, L4::Ipc::Iostream &ios)
{
  l4_msgtag_t tag;
  ios >> tag;

  if (tag.label() != L4Re::Dataspace::Protocol)
    return -L4_EBADPROTO;

  L4::Opcode op;
  ios >> op;

  //printf("op = %x\n", op);
  switch (op)
    {
    case L4Re::Dataspace_::Map:
        {
          bool read_only = !(obj & 1);
          l4_addr_t offset, spot;
          unsigned long flags;
          ios >> offset >> spot >> flags;

          L4::Ipc::Snd_fpage fp;
          long int ret = map(offset, spot, !read_only && (flags & 1), 0, ~0, fp);

          if (ret == L4_EOK)
            ios << fp;

          return ret;
        }
    case L4Re::Dataspace_::Stats:
      ios << _size << flags();
      return L4_EOK;
    default: // all others are not valid
      return -L4_ENOSYS;
    }
}

// ------------------------------------------------------------------------

class Fprov_server : public L4::Server_object_t<L4Re::Namespace>
{
public:
  Fprov_server();
  int dispatch(l4_umword_t obj, L4::Ipc::Iostream &ios);

private:
  enum {
    Max_filename_len = 1024,
    Buf_size = 64 << 10,
  };

  int get_file1(const char *fname, L4::Ipc::Iostream &ios);
  int get_file2(const char *fname, L4::Ipc::Iostream &ios);

  char *_buf;
};

Fprov_server::Fprov_server()
{
  _buf = (char *)malloc(Buf_size);
  if (!_buf)
    throw(L4::Out_of_memory());
}

int
Fprov_server::get_file2(char const *fname, L4::Ipc::Iostream &ios)
{
  printf("open: %s\n", fname);

  Ds *ds = new Ds(fname);

  ios << L4::Ipc::Snd_fpage(ds->obj_cap().fpage(L4_FPAGE_RO));
  return -L4_EOK;
}

int
Fprov_server::get_file1(char const *fname, L4::Ipc::Iostream &ios)
{
  gzFile fd = NULL;
  long fread;
  unsigned int fsize = 0;
  l4_addr_t addr;
  long ret;
  L4::Cap<L4Re::Dataspace> ds_cap;

  // XXX change to malloc
  //static char buf[64 << 10]; /* static because we have a limited stack */

  printf("open: %s\n", fname);

  // use buf for filename buffer
  if (fname[0] == '/')
    {
      snprintf(_buf, Buf_size, "/%s/%s", fprov_prefix, fname);
      _buf[Buf_size - 1] = 0;
      fd = gzopen(_buf, "r");
    }
  else
    for (int i = 0; i < nrpaths; ++i)
      {
        snprintf(_buf, Buf_size, "/%s/%s/%s", fprov_prefix, search_paths[i], fname);
        _buf[Buf_size - 1] = 0;
        printf("open: %s\n", _buf);
        if ((fd = gzopen(_buf, "r")))
          break;
      }

  if (fd == NULL)
    {
      printf("Can't open \"%s\": %s\n", fname, strerror(errno));
      return -L4_ENOENT;
    }

  /* Get the size of the _uncompressed_ file */
  while (1)
    {
      if ((fread = gzread(fd, _buf, Buf_size)) == -1)
        {
          printf("Error reading (or decoding) file %s: -%d\n", fname, errno);
          return -L4_EIO;
        }
      if (fread == 0)
        break;

      fsize += fread;
    }

  gzseek(fd, 0, SEEK_SET);

  ds_cap = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();
  if (!ds_cap.is_valid())
    {
      printf("Cap allocator failed\n");
      return -L4_ENOMEM;
    }

  if ((ret = L4Re::Env::env()->mem_alloc()->alloc(fsize, ds_cap)))
    {
      printf("Can't allocate dataspace with size %d (error %ld)\n",
             fsize, ret);
      L4Re::Util::cap_alloc.free(ds_cap, L4Re::This_task);
      gzclose(fd);
      return -L4_ENOMEM;
    }

  addr = 0;
  if ((ret = L4Re::Env::env()->rm()->attach(&addr, fsize,
                                            L4Re::Rm::Search_addr,
                                            ds_cap, 0)))
    {
      printf("Attach failed with error %ld\n", ret);
      L4Re::Util::cap_alloc.free(ds_cap, L4Re::This_task);
      gzclose(fd);
      return -L4_ENOMEM;
    }

  l4_addr_t pos = 0;
  while (pos < fsize)
    {
      if ((fread = gzread(fd, _buf, Buf_size)) == -1)
        {
          printf("Error reading file \"%s\": -%d\n", fname, errno);
          L4Re::Env::env()->rm()->detach(addr, 0);
          L4Re::Util::cap_alloc.free(ds_cap, L4Re::This_task);
          gzclose(fd);
          return -L4_EIO;
        }

      memcpy((void *)(addr + pos), _buf, fread);
      pos += fread;
    }

  L4Re::Env::env()->rm()->detach(addr, 0);
  gzclose(fd);

  ios << L4::Ipc::Snd_fpage(ds_cap.fpage(L4_FPAGE_RO));

  return 0;
}

int
Fprov_server::dispatch(l4_umword_t, L4::Ipc::Iostream &ios)
{
  l4_msgtag_t t;
  ios >> t;

  if (t.label() != L4Re::Namespace::Protocol)
    return -L4_EBADPROTO;

  l4_umword_t opcode;
  ios >> opcode;

  switch (opcode)
    {
    case L4Re::Namespace_::Query:
        {
          char filename[Max_filename_len];
          unsigned long len = Max_filename_len;
          ios >> L4::Ipc::buf_cp_in(filename, len);
          filename[len] = 0;

          return get_file1(filename, ios);
        }
      break;
    case L4Re::Namespace_::Register:
      printf("This fprov does not register anything, it's readonly!\n");
      return -L4_ENOSYS;
    default:
      return -L4_ENOSYS;
    };
}

int
main(int argc, char *argv[])
{
  try
    {
      setup(argc, argv);

      int err = mount("/", fprov_prefix, "fuxfs", 0, 0);
      if (err == -1)
        {
          perror("mount");
          exit(1);
        }

      static Fprov_server fprov;

      L4Re::chkcap(server.registry()->register_obj(&fprov, "rofs"), "register service", 0);

      server.loop();
    }
  catch (L4::Runtime_error const &e)
    {
      L4::cerr << e << "TERMINATED\n";
      abort();
    }
}

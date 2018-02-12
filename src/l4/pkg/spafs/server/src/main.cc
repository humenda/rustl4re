/*
 * (c) 2010 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *          Alexander Warg <warg@os.inf.tu-dresden.de>,
 *          Torsten Frenzel <frenzel@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/re/env>
#include <l4/re/dataspace>
#include <l4/re/namespace>
#include <l4/re/error_helper>
#include <l4/re/util/cap_alloc>
#include <l4/re/util/env_ns>
#include <l4/re/util/object_registry>
#include <l4/re/util/br_manager>
#include <l4/re/dataspace-sys.h>
#include <l4/re/namespace-sys.h>
#include <l4/re/mem_alloc>
#include <l4/re/util/meta>
#include <l4/sys/factory>
#include <l4/cxx/iostream>
#include <l4/cxx/list>

#include <l4/sys/cxx/ipc_epiface>
#include <l4/sys/cxx/ipc_server_loop>

#include <cstdlib>
#include <cstdio>
#include <cstring>

static bool verbose;

static L4Re::Util::Registry_server<L4Re::Util::Br_manager_hooks> server;

// ------------------------------------------------------------------------

class Fprov_server : public L4::Epiface_t<Fprov_server, L4Re::Namespace>
{
  struct Dir_entry : public cxx::List_item
  {
  public:
    typedef T_iter<Dir_entry> Iterator;

    Dir_entry()
    : name(0), addr(0), size(0), ds(L4_INVALID_CAP)
    {}

    char *name;
    unsigned name_len;
    unsigned long addr;
    unsigned long size;
    L4::Cap<L4Re::Dataspace> ds;
  };

public:
  Fprov_server(L4::Cap<L4Re::Dataspace> ds);

  // server interface ------------------------------------------
  int op_query(L4Re::Namespace::Rights,
               L4::Ipc::Array_in_buf<char, unsigned long> const &name,
               L4::Ipc::Snd_fpage &res, L4::Ipc::Opt<L4::Opcode> &,
               L4::Ipc::Opt<L4::Ipc::Array_ref<char, unsigned long> > &)
  {
    return get_file(name.data, name.length, res);
  }

  int op_link(L4Re::Namespace::Rights, unsigned,
                  L4::Ipc::Array_ref<char const, unsigned long> const &,
                  L4::Ipc::Snd_fpage,
                  L4::Ipc::Array_ref<char const, unsigned long> const &)
  { return -L4_EPERM; }

  int op_register_obj(L4Re::Namespace::Rights, unsigned,
                      L4::Ipc::Array_ref<char const, unsigned long> const &,
                      L4::Ipc::Snd_fpage &)
  { return -L4_EPERM; }

  int op_unlink(L4Re::Namespace::Rights,
                L4::Ipc::Array_ref<char const, unsigned long> const &)
  { return -L4_EPERM; }

private:
  enum
  {
    Max_filename_len = 1024,
  };

  int get_file(char const *filename, unsigned len, L4::Ipc::Snd_fpage &res);

  L4::Cap<L4Re::Dataspace> _ds;
  l4_addr_t _addr;
  Dir_entry *_dir;
};

Fprov_server::Fprov_server(L4::Cap<L4Re::Dataspace> ds)
: _ds(ds), _addr(0), _dir(0)
{
  int ret;

  _addr = 0;
  if ((ret = L4Re::Env::env()->rm()
               ->attach(&_addr, ds->size(), L4Re::Rm::Search_addr,
                        L4::Ipc::make_cap_rw(ds))))
    {
      printf("Attach of dataspace failed with error %d\n", ret);
      return;
    }

  printf("Extract metadata from archive\n");
  // create internal datastructure
  // FIXME: we need some sanity checks here, to prevent crashing by corrupted data
  unsigned *off = (unsigned *)_addr;
  do
    {
      if (*off == 0) break;

      Dir_entry *entry = new Dir_entry;
      entry->addr = (*off);
      off++;
      entry->size = (*off);
      off++;
      entry->name = (char *)(_addr + (*off));
      entry->name_len = strlen(entry->name);
      off++;
      if (verbose)
        printf("[0x%08lx: 0x%08lx] %s\n", entry->addr, entry->addr + entry->size, entry->name);

      _dir = cxx::List_item::push_back(_dir, entry);
    }
  while (1);
}

int
Fprov_server::get_file(char const *filename, unsigned len, L4::Ipc::Snd_fpage &res)
{
  if (0)
    printf("Open file '%.*s'\n", len, filename);
  Dir_entry *dir_entry = 0;
  // find the dir entry for the file
  for (Dir_entry::Iterator i = _dir; *i; ++i)
    {
      // compare from end
      if (i->name_len < len)
        continue;

      if (!strncmp(i->name + (i->name_len - len), filename, len))
	{
	  dir_entry = *i;
	  break;
	}
    }

  if (!dir_entry)
    {
      printf("File not found: <%.*s>\n", len, filename);
      return -L4_ENODEV;
    }

  if (dir_entry->ds.is_valid())
    {
      if (verbose)
        printf("file already open: <%.*s>\n", len, filename);
      res = L4::Ipc::Snd_fpage(dir_entry->ds.fpage(L4_FPAGE_RO));
      return 0;
    }

  L4::Cap<L4Re::Dataspace> file_ds = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();
  if (!file_ds.is_valid())
    {
      printf("Cannot allocate capability slot\n");
      return -L4_ENOMEM;
    }

  if (L4Re::Env::env()->mem_alloc()->alloc(dir_entry->size, file_ds, 0))
    {
      printf("Cannot allocate memory for file-dataspace\n");
      return -L4_ENODEV;
    }

  if (file_ds->size() != dir_entry->size)
    printf("Warning: file-dataspace has different size from file\n");

  if (file_ds->copy_in(0, _ds, dir_entry->addr, dir_entry->size))
    {
      printf("Cannot copy in file-dataspace from filesystem-dataspace\n");
      return -L4_ENODEV;
    }
  dir_entry->ds = file_ds;

  res = L4::Ipc::Snd_fpage(file_ds.fpage(L4_FPAGE_RO));

  return 0;
}


// ------------------------------------------------------------------------

class Fprov_service : public L4::Epiface_t<Fprov_service, L4::Factory>
{
  enum
  {
      Name_size = 256,
  };

public:
  Fprov_service() {};
  long op_create(L4::Factory::Rights, L4::Ipc::Cap<void> &res,
                   l4_umword_t type, L4::Ipc::Varg_list<> &&args)
  {
    if (type != 0 && type != L4Re::Namespace::Protocol)
      return -L4_ENODEV;

    L4::Ipc::Varg name = args.next();
    if (!name.is_of<char const *>())
      return -L4_EINVAL;

    Fprov_server *server = Fprov_service::create_server(name.value<char const *>());
    res = L4::Ipc::make_cap(server->obj_cap(), L4_CAP_FPAGE_RWSD);
    return L4_EOK;
  }

  static Fprov_server *create_server(char const *filename);
};

Fprov_server *
Fprov_service::create_server(const char *filename)
{
  printf("Create filesystem server for file:%s\n", filename);

  L4Re::Util::Env_ns ns;
  L4::Cap<L4Re::Dataspace> ds_cap = ns.query<L4Re::Dataspace>(filename);

  if (!ds_cap)
    {
      printf("Query filesystem dataspace failed\n");
      return 0;
    }

  return new Fprov_server(ds_cap);
}

static void setup(int argc, char *argv[])
{
  for (int i = 1; i < argc; i++)
    {
      if (!strcmp(argv[i], "-v"))
        verbose = true;

      // extract registry name
      char *reg_name = argv[i];
      char *fs_name = strstr(argv[i], ":");
      if (!fs_name)
        {
          printf("Invalid filesystem parameter string, "
                 "should be <registry-name:image-name>\n");
          continue;
        }
      *fs_name = 0;
      fs_name++;

      Fprov_server *server = Fprov_service::create_server(fs_name);

      if (!::server.registry()->register_obj(server, reg_name))
	printf("Failed to register filesystem under name '%s'\n", reg_name);
      else
	printf("Registered filesystem '%s' under %s:%s\n", fs_name, fs_name, reg_name);
    }
}

int
main(int argc, char *argv[])
{
  try
    {
      setup(argc, argv);

      static Fprov_service fprov;

      if (!server.registry()->register_obj(&fprov, "srv"))
        printf("Info: no 'srv' cap found, not registering generic service.\n");

      server.loop();
    }
  catch (L4::Runtime_error const &e)
    {
      L4::cerr << e << "terminated\n";
      abort();
    }
}

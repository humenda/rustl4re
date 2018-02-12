/*
 * (c) 2010 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *          Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/sys/capability>
#include <l4/re/env>
#include <l4/re/util/cap_alloc>
#include <l4/re/namespace>
#include <l4/sys/icu>
#include <l4/sys/iommu>
#include <l4/re/error_helper>
#include <l4/re/dataspace>
#include <l4/util/kip.h>
#include <l4/cxx/std_exc_io>

#include "main.h"
#include "hw_root_bus.h"
#include "hw_device.h"
#include "server.h"
#include "res.h"
#include "pci.h"
#include "platform_control.h"
#include "__acpi.h"
#include "virt/vbus.h"
#include "virt/vbus_factory.h"
#include "phys_space.h"
#include "ux.h"
#include "cfg.h"

#include <cstdio>
#include <typeinfo>
#include <algorithm>
#include <string>
#include <getopt.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "lua_glue.swg.h"

namespace {

static Hw::Root_bus *
hw_system_bus()
{
  static Hw::Root_bus _sb("System Bus");
  return &_sb;
}

static Platform_control *
platform_control()
{
  static Platform_control pfc(hw_system_bus());
  return &pfc;
}


struct Virtual_sbus : Vi::System_bus
{
  Virtual_sbus() : Vi::System_bus(platform_control()) {}
};

static Vi::Dev_factory_t<Virtual_sbus> __sb_root_factory("System_bus");

}

int luaopen_Io(lua_State *);

static const luaL_Reg libs[] =
{
  {"_G", luaopen_base },
  {LUA_LOADLIBNAME, luaopen_package},
  {LUA_STRLIBNAME, luaopen_string },
  {LUA_LOADLIBNAME, luaopen_package},
  {LUA_DBLIBNAME, luaopen_debug},
  {LUA_TABLIBNAME, luaopen_table},
  { NULL, NULL }
};

class Io_config_x : public Io_config
{
public:
  Io_config_x()
  : _do_transparent_msi(false), _verbose_lvl(1) {}

  bool transparent_msi(Hw::Device *) const
  { return _do_transparent_msi; }

  bool legacy_ide_resources(Hw::Device *) const
  { return true; }

  bool expansion_rom(Hw::Device *) const
  { return false; }

  void set_transparent_msi(bool v) { _do_transparent_msi = v; }

  int verbose() const { return _verbose_lvl; }
  void inc_verbosity() { ++_verbose_lvl; }

private:
  bool _do_transparent_msi;
  int _verbose_lvl;
};

static Io_config_x _my_cfg __attribute__((init_priority(30000)));
Io_config *Io_config::cfg = &_my_cfg;


Hw::Device *
system_bus()
{
  return hw_system_bus();
}

Hw_icu *
system_icu()
{
  static Hw_icu _icu;
  return &_icu;
}

template< typename X >
char const *type_name(X const &x)
{
  char *n = const_cast<char *>(typeid(x).name());
  strtol(n, &n, 0);
  return n;
}

static void dump(Device *d)
{
  Device::iterator i = Device::iterator(0, d, 100);
  for (; i != d->end(); ++i)
    {
      int indent = i->depth() * 2;
      if (dlevel(DBG_INFO))
        i->dump(indent);
      if (dlevel(DBG_DEBUG))
        {
          printf("%*.s  Resources: ==== start ====\n", indent, " ");
          for (Resource_list::const_iterator r = i->resources()->begin();
               r != i->resources()->end(); ++r)
            if (*r)
              (*r)->dump(indent + 2);
          printf("%*.s  Resources: ===== end =====\n", indent, " ");
        }
    }
}

static void check_conflicts(Hw::Device *d)
{
  for (auto i = Hw::Device::iterator(0, d, 100); i != d->end(); ++i)
    (*i)->check_conflicts();
}

void dump_devs(Device *d);
int add_vbus(Vi::Device *dev);

void dump_devs(Device *d) { dump(d); }


Hw_icu::Hw_icu()
{
  icu = L4Re::Env::env()->get_cap<L4::Icu>("icu");
  if (icu.is_valid())
    icu->info(&info);
}

int add_vbus(Vi::Device *dev)
{
  Vi::System_bus *b = dynamic_cast<Vi::System_bus*>(dev);
  if (!b)
    {
      d_printf(DBG_ERR, "ERROR: found non system-bus device as root device, ignored\n");
      return -1;
    }

  b->request_child_resources();
  b->allocate_pending_child_resources();
  b->setup_resources();
  if (!registry->register_obj(b, b->name()).is_valid())
    {
      d_printf(DBG_WARN, "WARNING: Service registration failed: '%s'\n", b->name());
      return -1;
    }
  if (dlevel(DBG_DEBUG2))
    dump(b);
  return 0;
}


struct Add_system_bus
{
  void operator () (Vi::Device *dev)
  {
    Vi::System_bus *b = dynamic_cast<Vi::System_bus*>(dev);
    if (!b)
      {
        d_printf(DBG_ERR, "ERROR: found non system-bus device as root device, ignored\n");
	return;
      }

    b->request_child_resources();
    b->allocate_pending_child_resources();
    b->setup_resources();
    if (!registry->register_obj(b, b->name()).is_valid())
      {
	d_printf(DBG_WARN, "WARNING: Service registration failed: '%s'\n", b->name());
	return;
      }
  }
};


static int
read_config(char const *cfg_file, lua_State *lua)
{
  d_printf(DBG_INFO, "Loading: config '%s'\n", cfg_file);

  char const *lua_err = 0;
  int err = luaL_loadfile(lua, cfg_file);

  switch (err)
    {
    case 0:
      if ((err = lua_pcall(lua, 0, LUA_MULTRET, 0)))
        {
          lua_err = lua_tostring(lua, -1);
          d_printf(DBG_ERR, "%s: error executing lua config: %s\n", cfg_file, lua_err);
          lua_pop(lua, lua_gettop(lua));
          return -1;
        }
      lua_pop(lua, lua_gettop(lua));
      return 0;
      break;
    case LUA_ERRSYNTAX:
      lua_err = lua_tostring(lua, -1);
      lua_pop(lua, lua_gettop(lua));
      break;
    case LUA_ERRMEM:
      d_printf(DBG_ERR, "%s: out of memory while loading file\n", cfg_file);
      exit(1);
    case LUA_ERRFILE:
      lua_err = lua_tostring(lua, -1);
      d_printf(DBG_ERR, "%s: cannot open/read file: %s\n", cfg_file, lua_err);
      exit(1);
    default:
      d_printf(DBG_ERR, "%s: unknown error: %s\n", cfg_file, lua_err);
      exit(1);
    }

  d_printf(DBG_ERR, "%s: error using as lua config: %s\n", cfg_file, lua_err);
  return -1;
}



static int
arg_init(int argc, char * const *argv, Io_config_x *cfg)
{
  while (1)
    {
      int optidx = 0;
      int c;
      enum
      {
        OPT_TRANSPARENT_MSI   = 1,
        OPT_TRACE             = 2,
        OPT_ACPI_DEBUG        = 3,
      };

      struct option opts[] =
      {
        { "verbose",           0, 0, 'v' },
        { "transparent-msi",   0, 0, OPT_TRANSPARENT_MSI },
        { "trace",             1, 0, OPT_TRACE },
        { "acpi-debug-level",  1, 0, OPT_ACPI_DEBUG },
        { 0, 0, 0, 0 },
      };

      c = getopt_long(argc, argv, "v", opts, &optidx);
      if (c == -1)
        break;

      switch (c)
        {
        case 'v':
          cfg->inc_verbosity();
          break;
        case OPT_TRANSPARENT_MSI:
	  printf("Enabling transparent MSIs\n");
          cfg->set_transparent_msi(true);
          break;
        case OPT_TRACE:
          {
            unsigned trace_mask = strtol(optarg, 0, 0);
            set_trace_mask(trace_mask);
            printf("Set trace mask to 0x%08x\n", trace_mask);
            break;
          }
        case OPT_ACPI_DEBUG:
          {
            l4_uint32_t acpi_debug_level = strtol(optarg, 0, 0);
            acpi_set_debug_level(acpi_debug_level);
            printf("Set acpi debug level to 0x%08x\n", acpi_debug_level);
            break;
          }
        }
    }
  return optind;
}

class Dma_domain_phys : public Dma_domain
{
public:
  int create_managed_kern_dma_space() override
  { return -L4_ENODEV; }

  int set_dma_task(bool, L4::Cap<L4::Task>) override
  { return -L4_ENODEV; }
};

static
int
run(int argc, char * const *argv)
{
  int argfileidx = arg_init(argc, argv, &_my_cfg);

  printf("Io service\n");
  set_debug_level(Io_config::cfg->verbose());

  d_printf(DBG_INFO, "Verboseness level: %d\n", Io_config::cfg->verbose());

  res_init();

  if (dlevel(DBG_DEBUG))
    Phys_space::space.dump();

  L4::Cap<L4::Iommu> iommu = L4Re::Env::env()->get_cap<L4::Iommu>("iommu");
  if (!iommu || !iommu.validate().label())
    {
      d_printf(DBG_INFO, "no 'iommu' capability found, using CPU-phys for DMA\n");
      Dma_domain *d = new Dma_domain_phys;
      system_bus()->add_resource(d);
      system_bus()->set_downstream_dma_domain(d);
    }

#if defined(ARCH_x86) || defined(ARCH_amd64)
  bool is_ux = l4util_kip_kernel_is_ux(l4re_kip());
  //res_get_ioport(0xcf8, 4);
  res_get_ioport(0, 16);

  if (!is_ux)
    acpica_init();
#endif

  system_bus()->plugin();

#if defined(ARCH_x86) || defined(ARCH_amd64)
  if (is_ux)
    ux_setup(system_bus());
#endif

  lua_State *lua = luaL_newstate();

  if (!lua)
    {
      printf("ERROR: cannot allocate Lua state\n");
      exit(1);
    }

  lua_newtable(lua);
  lua_setglobal(lua, "Io");

  for (int i = 0; libs[i].func; ++i)
    {
      luaL_requiref(lua, libs[i].name, libs[i].func, 1);
      lua_pop(lua, 1);
    }

  luaopen_Io(lua);

  extern char const _binary_io_lua_start[];
  extern char const _binary_io_lua_end[];

  if (luaL_loadbuffer(lua, _binary_io_lua_start,
                      _binary_io_lua_end - _binary_io_lua_start,
                      "@io.lua"))
    {
      d_printf(DBG_ERR, "INTERNAL: lua error: %s.\n", lua_tostring(lua, -1));
      lua_pop(lua, lua_gettop(lua));
      return 1;
    }

  if (lua_pcall(lua, 0, 1, 0))
    {
      d_printf(DBG_ERR, "INTERNAL: lua error: %s.\n", lua_tostring(lua, -1));
      lua_pop(lua, lua_gettop(lua));
      return 1;
    }

  for (; argfileidx < argc; ++argfileidx)
    read_config(argv[argfileidx], lua);

  if (dlevel(DBG_DEBUG))
    {
      printf("Real Hardware -----------------------------------\n");
      dump(system_bus());
    }

  check_conflicts(system_bus());

  if (!registry->register_obj(platform_control(), "platform_ctl"))
    d_printf(DBG_WARN, "warning: could not register control interface at"
                       " cap 'platform_ctl'\n");

  fprintf(stderr, "Ready. Waiting for request.\n");
  server_loop();

  return 0;
}

int
main(int argc, char * const *argv)
{
  try
    {
      return run(argc, argv);
    }
  catch (L4::Runtime_error &e)
    {
      std::cerr << "FATAL uncaught exception: " << e
                << "\nterminating...\n";

    }
  catch (...)
    {
      std::cerr << "FATAL uncaught exception of unknown type\n"
                << "terminating...\n";
    }
  return -1;
}

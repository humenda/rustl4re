/*
 * (c) 2012-2013 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>,
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

// TODO: compressed output (maybe 'dump' command, or cat -z or so)
// TODO: store buffer compressed
// TODO: port libxz (http://tukaani.org/xz/)
// TODO: macro to inject some text into the read buffer

#include "mux_impl.h"
#include "frontend.h"
#include "client.h"
#include "controller.h"
#include "debug.h"
#include "vcon_client.h"
#include "vcon_fe.h"
#include "async_vcon_fe.h"
#include "registry.h"
#include "server.h"

#include <l4/re/util/icu_svr>
#include <l4/re/util/vcon_svr>
#include <l4/re/util/object_registry>
#include <l4/re/util/br_manager>
#include <l4/re/error_helper>

#include <l4/sys/typeinfo_svr>
#include <l4/cxx/iostream>
#include <l4/sys/cxx/ipc_epiface>

#include <algorithm>
#include <set>
#include <getopt.h>

#ifdef USE_ASYNC_FE
typedef Async_vcon_fe Fe;
#else
typedef Vcon_fe Fe;
#endif

using L4Re::chksys;

struct Config_opts
{
  bool default_show_all;
  bool default_keep;
  std::string auto_connect_console;
};

static Config_opts config;


static L4::Server<L4Re::Util::Br_manager_hooks>  server(l4_utcb());
static Registry registry(&server);

class My_mux : public Mux_i, public cxx::H_list_item
{
public:
  My_mux(Controller *ctl, char const *name) : Mux_i(ctl, name) {}

  void add_auto_connect_console(std::string const &name)
  {
    _auto_connect_consoles.insert(name);
  }

  bool is_auto_connect_console(std::string const &name)
  {
    return _auto_connect_consoles.find(name) != _auto_connect_consoles.end();
  }

private:
  std::set<std::string> _auto_connect_consoles;
};

class Cons_svr : public L4::Epiface_t<Cons_svr, L4::Factory, Server_object>
{
public:
  explicit Cons_svr(const char *name);

  bool collected() { return false; }

  int create(cxx::String const &name, int color, Vcon_client **,
             size_t bufsz, Client::Key key);
  int op_create(L4::Factory::Rights, L4::Ipc::Cap<void> &obj,
                l4_mword_t proto, L4::Ipc::Varg_list_ref args);

  Controller *ctl() { return &_ctl; }
  void add(My_mux *m)
  {  _muxe.add(m); }

  int sys_msg(char const *fmt, ...) __attribute__((format(printf, 2, 3)));

private:
  typedef cxx::H_list<My_mux> Mux_list;
  typedef Mux_list::Iterator Mux_iter;
  Mux_list _muxe;
  Controller _ctl;
  Dbg _info;
  Dbg _err;
};

Cons_svr::Cons_svr(const char* name)
: _info(Dbg::Info, name), _err(Dbg::Err, name)
{
  _err.set_level(~0U);
  _info.set_level(~0U);
}

int
Cons_svr::sys_msg(char const *fmt, ...)
{
  int r = 0;
  for (Mux_iter i = _muxe.begin(); i != _muxe.end(); ++i)
    {
      va_list args;
      va_start(args, fmt);
      r = i->vsys_msg(fmt, args);
      va_end(args);
    }
  return r;
}

int
Cons_svr::create(cxx::String const &name, int color,
                 Vcon_client **vout, size_t bufsz, Client::Key key)
{
  typedef Controller::Client_iter Client_iter;
  Client_iter c = std::find_if(_ctl.clients.begin(),
                               Client_iter(_ctl.clients.end()),
                               Client::Equal_key(key));
  if (c != _ctl.clients.end())
    sys_msg("WARNING: multiple clients with key '%c'\n", key.v());

  cxx::String _name = name;
  if (_name.len() <= 0)
    _name = "<noname>";

  c = std::find_if(_ctl.clients.begin(), Client_iter(_ctl.clients.end()),
                   Client::Equal_tag(_name));


  Vcon_client *v = new Vcon_client(std::string(_name.start(), _name.len()),
                                   color, bufsz, key, &registry);
  if (!v)
    return -L4_ENOMEM;

  if (!registry.register_obj(v))
    {
      delete v;
      return -L4_ENOMEM;
    }

  if (c != _ctl.clients.end())
    {
      sys_msg("WARNING: multiple clients with tag '%.*s'\n",
              _name.len(), _name.start());
      v->idx = c->idx + 1;
      _ctl.clients.insert_before(v, c);
    }
  else
    _ctl.clients.push_front(v);

  sys_msg("Created vcon channel: %s [%lx]\n",
          v->tag().c_str(), v->obj_cap().cap());

  *vout = v;
  return 0;
}

int
Cons_svr::op_create(L4::Factory::Rights, L4::Ipc::Cap<void> &obj,
                    l4_mword_t proto, L4::Ipc::Varg_list_ref args)
{
  switch (proto)
    {
    case (l4_mword_t)L4_PROTO_LOG:
        {
          // copied from moe/server/src/alloc.cc

          L4::Ipc::Varg tag = args.next();

          if (!tag.is_of<char const *>())
            return -L4_EINVAL;

          L4::Ipc::Varg col = args.next();

          int color;
          if (col.is_of<char const *>())
            {
              cxx::String cs = cxx::String(col.value<char const*>(),
                                           col.length() - 1);

              int c = 7, bright = 0;
              if (!cs.empty())
                {
                  switch (cs[0])
                    {
                    case 'N': bright = 1; /* FALLTHRU */ case 'n': c = 0; break;
                    case 'R': bright = 1; /* FALLTHRU */ case 'r': c = 1; break;
                    case 'G': bright = 1; /* FALLTHRU */ case 'g': c = 2; break;
                    case 'Y': bright = 1; /* FALLTHRU */ case 'y': c = 3; break;
                    case 'B': bright = 1; /* FALLTHRU */ case 'b': c = 4; break;
                    case 'M': bright = 1; /* FALLTHRU */ case 'm': c = 5; break;
                    case 'C': bright = 1; /* FALLTHRU */ case 'c': c = 6; break;
                    case 'W': bright = 1; /* FALLTHRU */ case 'w': c = 7; break;
                    default: c = 0;
                    }
                }

              color = (bright << 3) | c;

            }
          else if (col.is_of_int())
            color = col.value<l4_mword_t>();
          else
            color = 7;

          char n[tag.length() + 1];
          memcpy(n, tag.value<char const*>(), tag.length());
          n[sizeof(n) - 1] = 0;

          bool show = config.default_show_all;
          bool keep = config.default_keep;
          Client::Key key;
          size_t bufsz = 0;

          for (L4::Ipc::Varg opts = args.next(); !opts.is_nil(); opts = args.next())
            {
              if (opts.is_of<char const *>())
                {
                  cxx::String cs = cxx::String(opts.value<char const *>(),
                                               opts.length() - 1);

                  if (cs == "hide")
                    show = false;
                  else if (cs == "show")
                    show = true;
                  else if (cs == "keep")
                    keep = true;
                  else if (cxx::String::Index k = cs.starts_with("key="))
                    key = *k;
                  else if (cxx::String::Index v = cs.starts_with("bufsz="))
                    cs.substr(v).from_dec(&bufsz);
                }
            }

          Vcon_client *v;
          if (int r = create(n, color, &v, bufsz, key))
            return r;

          if (show && _muxe.front())
            _muxe.front()->show(v);

          if (keep)
            v->keep(v);

          for (Mux_iter i = _muxe.begin(); i != _muxe.end(); ++i)
            {
              if (i->is_auto_connect_console(n))
                {
                  i->connect(v);
                  break;
                }
            }

          obj = L4::Ipc::make_cap(v->obj_cap(), L4_CAP_FPAGE_RWSD);
          return L4_EOK;
        }
      break;
    default:
      return -L4_ENOSYS;
    }
}


static int work(int argc, char const *argv[])
{
  printf("Console Server\n");

  enum {
    OPT_SHOW_ALL = 'a',
    OPT_MUX = 'm',
    OPT_FE = 'f',
    OPT_KEEP = 'k',
    OPT_AUTOCONNECT = 'c',
    OPT_DEFAULT_NAME = 'n',
    OPT_DEFAULT_BUFSIZE = 'B',
  };

  static option opts[] =
  {
      { "show-all", 0, 0, OPT_SHOW_ALL },
      { "mux", 1, 0, OPT_MUX },
      { "frontend", 1, 0, OPT_FE },
      { "keep", 0, 0, OPT_KEEP },
      { "autoconnect", 1, 0, OPT_AUTOCONNECT },
      { "defaultname", 1, 0, OPT_DEFAULT_NAME },
      { "defaultbufsize", 1, 0, OPT_DEFAULT_BUFSIZE },
      { 0, 0, 0, 0 },
  };

  Cons_svr *cons = new Cons_svr("cons");

  if (!registry.register_obj(cons, "cons"))
    {
      printf("Registering 'cons' server failed!\n");
      return 1;
    }

  My_mux *current_mux = 0;
  Fe *current_fe = 0;
  typedef std::set<std::string> Str_vector;
  Str_vector ac_consoles;
  const char *default_name = "cons";

  while (1)
    {
      int optidx = 0;
      int c = getopt_long(argc, const_cast<char *const*>(argv),
                          "am:f:kc:n:B:", opts, &optidx);
      if (c == -1)
        break;

      switch (c)
        {
        case OPT_SHOW_ALL:
          config.default_show_all = true;
          break;
        case OPT_MUX:
          current_mux = new My_mux(cons->ctl(), optarg);
          cons->add(current_mux);
          if (!ac_consoles.empty())
            printf("WARNING: Ignoring all previous auto-connect-consoles.\n");
          break;
        case OPT_FE:
          if (!current_mux)
            {
              printf("ERROR: need to instantiate a muxer (--mux) before\n"
                     "using the --frontend option.\n");
              break;
            }

            {
              L4::Cap<L4::Vcon> cap
                = L4Re::Env::env()->get_cap<L4::Vcon>(optarg);

              if (!cap)
                {
                  printf("ERROR: Frontend capability '%s' invalid.\n", optarg);
                  break;
                }

              current_fe = new Fe(cap, &registry);
              current_mux->add_frontend(current_fe);
            }
          break;
        case OPT_KEEP:
          config.default_keep = true;
          break;
        case OPT_AUTOCONNECT:
          if (current_mux)
            current_mux->add_auto_connect_console(optarg);
          else
            ac_consoles.insert(optarg);
          break;
        case OPT_DEFAULT_NAME:
          default_name = optarg;
          break;
        case OPT_DEFAULT_BUFSIZE:
          Vcon_client::default_obuf_size(atoi(optarg));
          break;
        }
    }

  // now check if we had any explicit options
  if (!current_mux)
    {
      current_mux = new My_mux(cons->ctl(), default_name);
      cons->add(current_mux);
      current_fe = new Fe(L4Re::Env::env()->log(), &registry);
      current_mux->add_frontend(current_fe);
      for (Str_vector::const_iterator i = ac_consoles.begin();
           i != ac_consoles.end(); ++i)
        current_mux->add_auto_connect_console(*i);
    }

  ac_consoles.clear();

  server.loop<L4::Runtime_error>(&registry);
  return 0;
}


int main(int argc, char const *argv[])
{
  try
    {
      return work(argc, argv);
    }
  catch (L4::Runtime_error const &e)
    {
      L4::cout << e;
      return 1;
    }
}

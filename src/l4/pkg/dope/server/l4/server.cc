/*
 * \brief   DOpE server module
 * \date    2002-11-13
 * \author  Norman Feske <nf2@inf.tu-dresden.de>
 *
 * This module is the main communication interface
 * between DOpE clients and DOpE.
 */

/*
 * Copyright (C) 2002-2004  Norman Feske  <nf2@os.inf.tu-dresden.de>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the DOpE package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */

/*** GENERAL ***/
#include <cstdlib>
#include <cstdio>
#include <ctype.h>

/*** DOPE SPECIFIC ***/
#include "dopestd.h"
#include "thread.h"
#include "server.h"
#include "appman.h"
#include "script.h"
#include "scope.h"
#include "screen.h"
#include "messenger.h"
#include "userstate.h"
#include "scheduler.h"
#include <l4/dope/dopedef.h>

#include <l4/re/env>
#include <l4/re/namespace>
#include <l4/re/util/cap_alloc>
#include <l4/sys/factory>
#include <l4/sys/typeinfo_svr>
#include <l4/cxx/exceptions>
#include <l4/sys/cxx/ipc_epiface>

#include <l4/cxx/minmax>

#include <l4/re/console>
#include <l4/re/util/object_registry>
#include <l4/re/util/video/goos_svr>
#include <l4/re/util/event_svr>
#include <l4/re/util/event_buffer>
#include <l4/re/util/br_manager>
#include <l4/re/c/event.h>
#include <l4/re/video/goos>
#include <l4/cxx/string>

#include <l4/sys/debugger.h>
#include <pthread-l4.h>

static struct userstate_services *userstate;
static struct thread_services    *thread;
static struct appman_services    *appman;
static struct script_services    *script;
static struct scope_services     *scope;
static struct screen_services    *screen;
static struct scheduler_services *scheduler;
static struct messenger_services *msg;

extern "C" int init_server(struct dope_services *d);

static int get_string(const char *configstr, const char *var,
                      char **val, unsigned long *vallen)
{
  char *a;
  a = strstr(configstr, var);
  if (!a)
    return 0;
  a += strlen(var);
  *val = a;
  while (*a && !isspace(*a))
    a++;
  *vallen = a - *val;
  return 1;
}

// ------------------------------------------------------------------------
static L4Re::Util::Object_registry *dope_registry;

// ------------------------------------------------------------------------

class Dope_base : public L4Re::Util::Event_svr<Dope_base>
{
protected:
  s32 app_id;
  L4Re::Util::Event_buffer evbuf;

  int cmd(const char *c);
  int cmd_ret(const char *c, char *result, unsigned long res_len);
  int create_event();

  bool register_appid(s32 ai, Dope_base *obj);

public:
  void event_cb(l4re_event_t *e, unsigned nr_events);
  static Dope_base *get_obj(s32 ai);
  void reset_event_buffer() { evbuf.reset(); }

  virtual L4::Ipc_svr::Server_iface *server_iface() const = 0;

private:
  static void check_appid(s32 ai);
  static Dope_base *appid_to_obj[MAX_APPS];
};

Dope_base *Dope_base::appid_to_obj[MAX_APPS];

void Dope_base::check_appid(s32 ai)
{
  if (ai >= MAX_APPS)
    {
      printf("Oops: %d >= MAX_APPS\n", ai);
      exit(1);
    }
}

bool Dope_base::register_appid(s32 ai, Dope_base *obj)
{
  check_appid(ai);
  appid_to_obj[ai] = obj;
  return true;
}

Dope_base *Dope_base::get_obj(s32 ai)
{
  check_appid(ai);
  return appid_to_obj[ai];
}

extern "C" void dope_event_inject(s32 appid, l4re_event_t *e,
                                  unsigned nr_events);

void dope_event_inject(s32 appid, l4re_event_t *e, unsigned nr_events)
{
  Dope_base::get_obj(appid)->event_cb(e, nr_events);
}

int Dope_base::cmd(const char *c)
{
  INFO(printf("Server(exec_cmd): cmd %s execution requested by app_id=%u\n", c, (u32)app_id);)
  appman->lock(app_id);
  int ret = script->exec_command(app_id, (char *)c, NULL, 0);
  appman->unlock(app_id);
  if (ret < 0)
    printf("DOpE(exec_cmd): Error - command \"%s\" returned %d\n", c, ret);
  return ret;
}

int Dope_base::cmd_ret(const char *c, char *result, unsigned long res_len)
{
  INFO(printf("Server(exec_req): cmd %s execution requested by app_id=%u\n",
              c, (u32)app_id);)
  appman->lock(app_id);
  result[0] = 0;
  int ret = script->exec_command(app_id, (char *)c, result, res_len);
  appman->unlock(app_id);
  INFO(printf("Server(exec_req): send result msg: %s\n", result));

  if (ret < 0) printf("DOpE(exec_req): Error - command \"%s\" returned %d\n", c, ret);
  return ret;
}

int
Dope_base::create_event()
{
  long r;

  auto b = L4Re::Util::make_unique_cap<L4Re::Dataspace>();
  if (!b.is_valid())
    return -L4_ENOMEM;

  if ((r = L4Re::Env::env()->mem_alloc()->alloc(L4_PAGESIZE, b.get())) < 0)
    return r;

  if ((r = evbuf.attach(b.get(), L4Re::Env::env()->rm())) < 0)
    return r;

  memset(evbuf.buf(), 0, b.get()->size());

  _ds  = b.release();

  return 0;
}

void Dope_base::event_cb(l4re_event_t *e, unsigned nr_events)
{
#if 0
  printf("emmit event (client=%p): event (t=%d c=%d v=%d stream=%lx)\n",
         this, e->type, e->code, e->value, e->stream_id);
#endif
  for (unsigned i = 0; i < nr_events; ++i)
    evbuf.put(*reinterpret_cast<L4Re::Event_buffer::Event const*>(&e[i]));
  _irq.trigger();
}


// ------------------------------------------------------------------------

class Dope_app :
  public Dope_base,
  public L4::Epiface_t<Dope_app, Dope::Dope>
{
public:
  explicit Dope_app(const char *configstr);

  L4::Ipc_svr::Server_iface *server_iface() const
  { return L4::Epiface::server_iface(); }

  long op_c_cmd(Dope::Dope::Rights, L4::Ipc::Array_in_buf<char, unsigned long> const &c)
  { return this->cmd(c.data); }

  long op_c_cmd_req(Dope::Dope::Rights,
                    L4::Ipc::Array_in_buf<char, unsigned long> const &c,
                    L4::Ipc::Array_ref<char, unsigned long> &res)
  {
    cmd_ret(c.data, res.data, res.length);
    res.length = strlen(res.data);
    return 0;
  }

  long op_c_vscreen_get_fb(Dope::Dope::Rights,
                           L4::Ipc::Array<char const, unsigned long> const &s,
                           L4::Ipc::Cap<L4Re::Dataspace> &ds)
  {
    char buf[40];
    char res[50];

    snprintf(buf, sizeof(buf), "%s.map()", s.data);
    cmd_ret(buf, res, sizeof(res));
    l4_cap_idx_t x = strtoul(&res[3], NULL, 0);
    ds = L4::Ipc::make_cap(L4::Cap<L4Re::Dataspace>(x), L4_CAP_FPAGE_RW);
    return L4_EOK;
  }

  long op_c_get_keystate(Dope::Dope::Rights, long key, long &res)
  {
    res = userstate->get_keystate(key);
    return -L4_EOK;
  }
};

Dope_app::Dope_app(const char *configstr)
{
  dope_registry->register_obj(this);

  unsigned long val;
  char *s;
  char buf[80];
  const char *appname = "App";

  if (get_string(configstr, "name=", &s, &val))
    {
      strncpy(buf, s, val);
      buf[val] = 0;
      appname = buf;
    }

  if (int r = create_event())
    throw (L4::Runtime_error(r));

  app_id = appman->reg_app(appname);
  register_appid(app_id, this);

  SCOPE *rootscope = scope->create();
  appman->set_rootscope(app_id, rootscope);
  INFO(printf("Server(init_app): application init request. appname=%s\n", appname));
}

// ------------------------------------------------------------------------

class Dope_fb :
  public L4Re::Util::Video::Goos_svr,
  public Dope_base,
  public L4::Epiface_t<Dope_fb, L4Re::Console>
{
public:
  using L4Re::Util::Video::Goos_svr::op_info;
  using Dope_base::op_info;

  explicit Dope_fb(L4::Ipc::Varg_list_ref is);
  L4::Ipc_svr::Server_iface *server_iface() const
  { return L4::Epiface::server_iface(); }

  virtual int refresh(int x, int y, int w, int h);
};

Dope_fb::Dope_fb(L4::Ipc::Varg_list_ref is)
{
  unsigned xpos = 20, ypos = 20;
  char appname[80] = "fb";

  _screen_info.width     = 300;
  _screen_info.height    = 200;

  dope_registry->register_obj(this);

  for (L4::Ipc::Varg opt = is.next(); !opt.is_nil(); opt = is.next())
    {
      if (!opt.is_of<char const *>())
        {
          printf("skipping non string argument for session!\n");
          continue;
        }

      cxx::String a(opt.value<char const *>(), opt.length());

      cxx::String::Index v;
      if (   (v = a.starts_with("g="))
          || (v = a.starts_with("geometry=")))
        {
          a = a.substr(v);
          int r = a.from_dec(&_screen_info.width);
          if (r >= a.len() || a[r] != 'x')
            {
              printf("Invalid geometry format\n");
              continue;
            }
          a = a.substr(r + 1);
          r = a.from_dec(&_screen_info.height);

          if (r < a.len() && a[r] == '+')
            {
              a = a.substr(r + 1);
              r = a.from_dec(&xpos);
            }

          if (r < a.len() && a[r] == '+')
            {
              a = a.substr(r + 1);
              r = a.from_dec(&ypos);
            }

          if (_screen_info.width <= 0 || _screen_info.height <= 0
              || _screen_info.width >= 10000
              || _screen_info.height >= 10000)
            {
              printf("invalid geometry (too big)\n");
            }
        }
      else if (   (v = a.starts_with("l="))
               || (v = a.starts_with("label=")))
        {
          a = a.substr(v);
          strncpy(appname, v, a.end() - v);
          appname[a.end() - v] = 0;
        }
    }

  L4Re::Video::Pixel_info pixinfo(16, 5, 11, 6, 5, 5, 0);
  pixinfo.bytes_per_pixel(2);

  _screen_info.flags        = L4Re::Video::Goos::F_pointer;
  _screen_info.num_static_views   = 1;
  _screen_info.num_static_buffers = 1;
  _screen_info.pixel_info   = pixinfo;

  init_infos();
  _view_info.bytes_per_line = _screen_info.width * 2;
  _view_info.buffer_offset  = 0;

  app_id = appman->reg_app(appname);
  register_appid(app_id, this);

  SCOPE *rootscope = scope->create();
  //THREAD *listener_thread = thread->alloc_thread();
  appman->set_rootscope(app_id, rootscope);

  INFO(printf("Server(init_fb): fb init request. appname=%s\n", appname));

  //thread->ident2thread(listener, listener_thread);
  //appman->reg_list_thread(app_id, listener_thread);
  //appman->reg_listener(app_id, (void *)&listener_thread->tid);

  //appman->reg_app_thread(app_id, (THREAD *)&client_thread);

  if (int r = create_event())
    throw (L4::Runtime_error(r));

  char buf[80];
  cmd("x=new Window()");
  cmd("y=new VScreen()");
  snprintf(buf, sizeof(buf), "y.setmode(%ld,%ld,\"RGB16\")",
                             _screen_info.width, _screen_info.height);
  buf[sizeof(buf)-1] = 0;
  cmd(buf);

  snprintf(buf, sizeof(buf), "x.set(-x %d -y %d -content y -workw %ld -workh %ld)",
                             xpos, ypos,
                             _screen_info.width, _screen_info.height);
  buf[sizeof(buf)-1] = 0;
  cmd(buf);

  snprintf(buf, sizeof(buf), "y.bind(\"press\",   1)");
  buf[sizeof(buf)-1] = 0;
  cmd(buf);

  snprintf(buf, sizeof(buf), "y.bind(\"release\",  1)");
  buf[sizeof(buf)-1] = 0;
  cmd(buf);

  snprintf(buf, sizeof(buf), "y.bind(\"motion\",   1)");
  buf[sizeof(buf)-1] = 0;
  cmd(buf);

  cmd("x.open()");

  cmd_ret("y.map()", buf, sizeof(buf));

  printf("y.map() = %s\n", buf);

  l4_cap_idx_t x = strtoul(&buf[3], NULL, 0);
  _fb_ds = L4::Cap<L4Re::Dataspace>(x);

//  printf("fb_ds = %lx\n", _fb_ds.cap());
//  printf("evbuf2 = %p\n", evbuf.buf());
}

int Dope_fb::refresh(int x, int y, int w, int h)
{
  char buf[70];
  // Hmm: y is a VSCREEN, so we could use y->vscr->refresh(y, ...)
  snprintf(buf, sizeof(buf), "y.refresh(-x %d -y %d -w %d -h %d)", x, y, w, h);
  buf[sizeof(buf) - 1] = 0;
  cmd(buf);
  return L4_EOK;
}

// ------------------------------------------------------------------------

class Controller : public L4::Epiface_t<Controller, L4::Factory>
{
public:
  long op_create(L4::Factory::Rights r, L4::Ipc::Cap<void> &obj,
                 l4_umword_t type, L4::Ipc::Varg_list<> &&args)
  {
    if (!(r & L4_CAP_FPAGE_S))
      return -L4_EPERM;

    switch (type)
      {
      default:
        printf("Invalid object type requested\n");
        return -L4_ENODEV;

      case L4Re::Video::Goos::Protocol:
          {
            Dope_fb *x = new Dope_fb(args);
            obj = L4::Ipc::make_cap(x->obj_cap(), L4_CAP_FPAGE_RWD);
            return L4_EOK;
          }
      case 0: // dope iface
          {
            L4::Ipc::Varg arg = args.next();
            if (!arg.is_of<char const*>())
              return -L4_EINVAL;
            unsigned long size = 100;
            char s[size];
            strncpy(s, arg.value<char const*>(), cxx::min<int>(size, arg.length()));
            s[size] = 0;
            Dope_app *x = new Dope_app(s);
            obj = L4::Ipc::make_cap(x->obj_cap(), L4_CAP_FPAGE_RWD);
            return L4_EOK;
          }
      }
  }
};

/*** DOpE SERVER THREAD ***/
static void *server_thread(void *)
{
  static Controller ctrl;

  l4_debugger_set_object_name(pthread_l4_cap(pthread_self()), "dope-srv");

  static L4::Server<L4Re::Util::Br_manager_hooks> dope_server(l4_utcb());
  dope_registry = new L4Re::Util::Object_registry
      (&dope_server,
       L4::Cap<L4::Thread>(pthread_l4_cap(pthread_self())),
       L4Re::Env::env()->factory());

  if (!dope_registry)
    return NULL;


  if (!dope_registry->register_obj(&ctrl, "dope").is_valid())
    {
      printf("Service registration failed.\n");
      return NULL;
    }

  INFO(printf("Server(server_thread): entering server loop\n"));
  dope_server.loop<L4::Runtime_error>(dope_registry);

  return NULL;
}


/*************************
 *** SERVICE FUNCTIONS ***
 *************************/

/*** START SERVING ***/
static void start(void)
{
	INFO(printf("Server(start): creating server thread\n");)
	thread->start_thread(NULL, &server_thread, NULL);
}


/****************************************
 *** SERVICE STRUCTURE OF THIS MODULE ***
 ****************************************/

static struct server_services services = {
	start,
};


/**************************
 *** MODULE ENTRY POINT ***
 **************************/

extern "C" int init_server(struct dope_services *d)
{
  thread    = (thread_services *)d->get_module("Thread 1.0");
  appman    = (appman_services *)d->get_module("ApplicationManager 1.0");
  script    = (script_services *)d->get_module("Script 1.0");
  msg       = (messenger_services *)d->get_module("Messenger 1.0");
  userstate = (userstate_services *)d->get_module("UserState 1.0");
  scope     = (scope_services *)d->get_module("Scope 1.0");
  screen    = (screen_services *)d->get_module("Screen 1.0");
  scheduler = (scheduler_services *)d->get_module("Scheduler 1.0");

  d->register_module("Server 1.0", &services);
  return 1;
}

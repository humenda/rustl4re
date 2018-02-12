/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>,
 *               Torsten Frenzel <frenzel@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/re/env>
#include <l4/re/namespace>
#include <l4/re/util/cap_alloc>
#include <l4/re/util/br_manager>

#include <l4/sys/capability>
#include <l4/sys/factory>
#include <l4/cxx/ipc_timeout_queue>
#include <l4/sys/cxx/ipc_epiface>
#include <l4/cxx/minmax>

#include <cstdio>
#include <l4/l4con/l4con.h>
#include <l4/input/libinput.h>

#include <l4/re/util/video/goos_svr>
#include <l4/re/util/video/goos_fb>
#include <l4/re/util/event_svr>
#include <l4/re/util/event_buffer>
#include <l4/re/util/cap_alloc>
#include <l4/re/util/unique_cap>
#include <l4/re/console>

#include "object_registry_gc"
#include "main.h"
#include "l4con.h"
#include "gmode.h"
#include "srv.h"
#include "vc.h"

// XXX: why is this in the L4Re::Util namespace ???
struct My_hooks :
  L4::Ipc_svr::Ignore_errors,
  L4::Ipc_svr::Timeout_queue_hooks<My_hooks, L4Re::Util::Br_manager>
{
  static l4_cpu_time_t now()
  { return l4_kip_clock(l4re_kip()); }
};

static L4::Server<My_hooks> con_server(l4_utcb());
static L4Re::Util::Object_registry_gc
                              con_registry(&con_server,
                                           L4Re::Env::env()->main_thread(),
                                           L4Re::Env::env()->factory());

class Vc : public L4Re::Util::Video::Goos_svr,
           public L4Re::Util::Event_svr<Vc>,
	   public L4::Epiface_t<Vc, L4con>,
	   public l4con_vc
{
public:
  using L4Re::Util::Video::Goos_svr::op_info;
  using L4Re::Util::Event_svr<Vc>::op_info;

  explicit Vc();
  ~Vc();

  void setup_info(l4con_vc *vc);
  void reg_fbds(l4_cap_idx_t c);
  void send_event(l4input *ev);
  int create_event();

  long close();

  long op_close(L4con::Rights)
  { return close(); }

  long op_pslim_fill(L4con::Rights, int x, int y, int w, int h,
                     l4con_pslim_color_t color)
  {
    l4con_pslim_rect_t r(x, y, w, h);
    return con_vc_pslim_fill_component(this, &r, color);
  }

  long op_pslim_copy(L4con::Rights, int x, int y, int w, int h,
                     l4_int16_t dx, l4_int16_t dy)
  {
    l4con_pslim_rect_t r(x, y, w, h);
    return con_vc_pslim_copy_component(this, &r, dx, dy);
  }

  long op_puts(L4con::Rights, short x, short y, l4con_pslim_color_t fg_color,
               l4con_pslim_color_t bg_color,
               L4::Ipc::Array_in_buf<char, unsigned long> const &text)
  {
      return con_vc_puts_component(this, text.data, text.length, x, y,
                                   fg_color, bg_color);
  }

  long op_puts_scale(L4con::Rights, short x, short y, l4con_pslim_color_t fg_color,
                     l4con_pslim_color_t bg_color, short scale_x, short scale_y,
                     L4::Ipc::Array_in_buf<char, unsigned long> const &text)
  {
      return con_vc_puts_scale_component(this, text.data, text.length, x, y,
                                         fg_color, bg_color, scale_x, scale_y);
  }

  long op_get_font_size(L4con::Rights, short &w, short &h)
  {
    w = FONT_XRES;
    h = FONT_YRES;
    return L4_EOK;
  }


  virtual int refresh(int x, int y, int w, int h);

  void reset_event_buffer() { evbuf.reset(); }
private:
  L4Re::Util::Event_buffer evbuf;
};

Vc::Vc()
{
}

int
Vc::refresh(int x, int y, int w, int h)
{
  l4con_pslim_rect_t r;
  r.x = x;
  r.y = y;
  r.w = w;
  r.h = h;
  return con_vc_direct_update_component(this, &r);
}

int
Vc::create_event()
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

long
Vc::close()
{
  return con_vc_close_component(this);
}

Vc::~Vc()
{
  con_vc_close_component(this);
}


extern "C" l4con_vc *alloc_vc()
{ return new Vc(); }

extern "C" void create_event(struct l4con_vc *vc)
{ ((Vc *)vc)->create_event(); }

void
Vc::setup_info(l4con_vc *vc)
{
  _screen_info.pixel_info           = *(L4Re::Video::Pixel_info *)&fb_info.pixel_info;
  _screen_info.width                = vc->client_xres;
  _screen_info.height               = vc->client_yres;
  _screen_info.flags                = 0;
  _screen_info.num_static_views     = 1;
  _screen_info.num_static_buffers   = 1;

  init_infos();

  _view_info.buffer_offset          = 0;
  _view_info.bytes_per_line         = vc->bytes_per_line;
}

void
Vc::send_event(l4input *ev)
{
  evbuf.put(*reinterpret_cast<L4Re::Event_buffer::Event const*>(ev));
  static_assert(sizeof(L4Re::Event_buffer::Event) == sizeof(*ev),
                "Size mismatch");
  _irq.trigger();
}

extern "C" void fill_out_info(l4con_vc *vc)
{ ((Vc *)vc)->setup_info(vc); }

extern "C" void send_event_client(struct l4con_vc *vc, l4input *ev)
{
  if (!ev->time)
    printf("WARNING: Emiting invalid event!\n");

  if (vc_mode & CON_IN)
    ((Vc *)vc)->send_event(ev);
}

extern "C" void register_fb_ds(struct l4con_vc *vc)
{ ((Vc *)vc)->reg_fbds(vc->vfb_ds); }


void
Vc::reg_fbds(l4_cap_idx_t c)
{
  L4::Cap<L4Re::Dataspace> t(c);
  _fb_ds = t;
}

class Controller : public L4::Epiface_t<Controller, L4::Factory>
{
public:
  long op_create(L4::Factory::Rights, L4::Ipc::Cap<void> &obj,
                 long type, L4::Ipc::Varg_list<> &&)
  {
    if (!L4::kobject_typeid<L4con>()->has_proto(type))
      return -L4_ENODEV;

    int connum = con_if_open_component(CON_VFB);
    if (connum < 0)
      return -L4_ENOMEM;

    con_registry.register_obj_with_gc((Vc *)vc[connum], 0);

    obj = L4::Ipc::make_cap(((Vc *)vc[connum])->obj_cap(), L4_CAP_FPAGE_RWSD);
    return L4_EOK;
  }
};

// ---------------------------------------------------------------
struct Periodic : L4::Ipc_svr::Timeout_queue::Timeout
{
  void expired()
  {
    periodic_work();
    con_registry.gc_run(500);
    con_server.queue.add(this, timeout() + REQUEST_TIMEOUT_DELTA);
  }
};

static Periodic periodic;

int server_loop(void)
{
  static Controller ctrl;
  con_server.queue.add(&periodic,
                       l4_kip_clock(l4re_kip()) + REQUEST_TIMEOUT_DELTA);

  if (!con_registry.register_obj(&ctrl, "con"))
    {
      printf("Service registration failed.\n");
      return 1;
    }

  printf("Ready. Waiting for clients\n");
  con_server.loop<L4::Runtime_error, L4Re::Util::Object_registry_gc &>(con_registry);
  return 0;
}

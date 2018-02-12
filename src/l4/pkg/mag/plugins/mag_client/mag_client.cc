/*
 * (c) 2010 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *          Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/re/env>
#include <l4/re/namespace>
#include <l4/re/event_enums.h>
#include <l4/re/event-sys.h>
#include <l4/re/error_helper>
#include <l4/re/rm>
#include <l4/re/dataspace>
#include <l4/re/util/video/goos_svr>
#include <l4/re/util/event_svr>
#include <l4/re/util/icu_svr>
#include <l4/re/util/unique_cap>
#include <l4/re/video/goos-sys.h>
#include <l4/re/video/goos>
#include <l4/re/console>
#include <l4/re/util/meta>

#include <l4/mag/server/plugin>
#include <l4/mag/server/object>
#include <l4/mag/server/session>
#include <l4/mag/server/user_state>
#include <l4/mag-gfx/clip_guard>
#include <l4/mag-gfx/texture>
#include <l4/mag-gfx/factory>

#include <l4/cxx/list>
#include <l4/cxx/unique_ptr>

#include <cstring>
#include <cstdio>
#include <memory>
#include <vector>
#include <list>

namespace Mag_server { namespace {

using L4Re::Util::Unique_cap;
using std::auto_ptr;
using Mag_gfx::Texture;
using Mag_gfx::Area;
using L4Re::chksys;

class Mag_client :
  public L4::Epiface_t<Mag_client, L4::Factory, Object>,
  private Plugin
{
private:
  Core_api const *_core;

public:
  Mag_client() : Plugin("Mag client") {}
  char const *type() const { return "Mag client"; }
  void start(Core_api *core);

  void destroy();
  long op_create(L4::Factory::Rights rights, L4::Ipc::Cap<void> &obj,
                 l4_mword_t proto, L4::Ipc::Varg_list<> const &args);
};

class Client_buffer;
class Client_view;

class Mag_goos
: public Session,
  public L4::Epiface_t<Mag_goos, L4Re::Console, Object>,
  public L4Re::Util::Icu_cap_array_svr<Mag_goos>
{
private:
  typedef L4Re::Util::Icu_cap_array_svr<Mag_goos> Icu_svr;
  typedef L4Re::Video::Goos::Rights Goos_rights;

  Core_api const *_core;
  L4Re::Util::Unique_cap<L4Re::Dataspace> _ev_ds;
  Irq _ev_irq;
  L4Re::Rm::Unique_region<void*> _ev_ds_m;
  L4Re::Event_buffer _events;

  typedef std::vector<cxx::Ref_ptr<Client_buffer> >  Buffer_vector;
  typedef std::vector<cxx::unique_ptr<Client_view> > View_vector;

  Buffer_vector _buffers;
  View_vector _views;

public:
  long op_info(Goos_rights, L4Re::Video::Goos::Info &i);

  long op_get_static_buffer(Goos_rights, unsigned idx,
                            L4::Ipc::Cap<L4Re::Dataspace> &ds);

  long op_create_buffer(Goos_rights, unsigned long, L4::Ipc::Cap<L4Re::Dataspace> &);

  long op_delete_buffer(Goos_rights, unsigned)
  { return -L4_ENOSYS; }

  long op_create_view(Goos_rights);

  long op_delete_view(Goos_rights, unsigned);

  long op_view_info(Goos_rights, unsigned idx, L4Re::Video::View::Info &info);

  long op_set_view_info(Goos_rights, unsigned, L4Re::Video::View::Info const &);

  long op_view_stack(Goos_rights, unsigned view, unsigned pivot, bool behind);

  long op_view_refresh(Goos_rights, unsigned idx, int x, int y, int w, int h);

  long op_refresh(Goos_rights, int x, int y, int w, int h);

  long op_get_buffer(L4Re::Event::Rights, L4::Ipc::Cap<L4Re::Dataspace> &ds)
  {
    _events.reset();
    ds = L4::Ipc::Cap<L4Re::Dataspace>(_ev_ds.get(), L4_CAP_FPAGE_RW);
    return L4_EOK;
  }

  long op_get_num_streams(L4Re::Event::Rights)
  { return -L4_ENOSYS; }

  long op_get_stream_info(L4Re::Event::Rights, int, L4Re::Event_stream_info &)
  { return -L4_ENOSYS; }

  long op_get_stream_info_for_id(L4Re::Event::Rights, l4_umword_t id,
                                 L4Re::Event_stream_info &info)
  { return _core->user_state()->get_input_stream_info_for_id(id, &info); }

  long op_get_axis_info(L4Re::Event::Rights, l4_umword_t id,
                        L4::Ipc::Array_in_buf<unsigned, unsigned long> const &axes,
                        L4::Ipc::Array_ref<L4Re::Event_absinfo, unsigned long> &info)
  {
    unsigned naxes = cxx::min<unsigned>(L4RE_ABS_MAX, axes.length);

    info.length = 0;

    L4Re::Event_absinfo _info[naxes];
    int r = _core->user_state()->get_input_axis_info(id, naxes, axes.data, _info, 0);
    if (r < 0)
      return r;

    for (unsigned i = 0; i < naxes; ++i)
      info.data[i] = _info[i];

    info.length = naxes;
    return r;
  }

  long op_get_stream_state_for_id(L4Re::Event::Rights, l4_umword_t,
                                  L4Re::Event_stream_state &)
  { return -L4_ENOSYS; }



public:
  Mag_goos(Core_api const *core);

  using Icu_svr::op_info;

  void put_event(Hid_report *e, bool trigger);
  void put_event(l4_umword_t stream, int type, int code, int value,
                 l4_uint64_t time);

  void destroy();

  static void set_default_background(Session *_s, Property_handler const *, cxx::String const &);
};

class Client_buffer : public cxx::Ref_obj
{
private:
  L4Re::Util::Unique_cap<L4Re::Dataspace> _ds;
  L4Re::Rm::Unique_region<void *> _texture_mem;
  unsigned long _size;

public:
  unsigned index;

  Client_buffer(Core_api const *core, unsigned long size);

  L4::Cap<L4Re::Dataspace> ds_cap() const { return _ds.get(); }
  void *local_addr() const { return _texture_mem.get(); }
  unsigned long size() const { return _size; }
};


class Client_view : public View
{
private:
  Core_api const *_core;
  cxx::Ref_ptr<Client_buffer> _buffer;
  Mag_goos *_screen;
  Texture *_front_txt;
  Texture *_back_txt;

  unsigned long _buf_offset;

  void swap_textures()
  {
    Texture *tmp = _front_txt;
    asm volatile ("" : : : "memory");
    _front_txt = _back_txt;
    _back_txt = tmp;
  }

public:
  Client_view(Core_api const *core, Mag_goos *screen);
  virtual ~Client_view();

  void draw(Canvas *, View_stack const *, Mode) const;
  void handle_event(Hid_report *e, Point const &mouse, bool core_dev);

  void get_info(L4Re::Video::View::Info *inf) const;
  void set_info(L4Re::Video::View::Info const &inf,
                cxx::Ref_ptr<Client_buffer> const &b);

  Session *session() const { return _screen; }
};

Client_view::Client_view(Core_api const *core, Mag_goos *screen)
: View(Rect(), F_need_frame), _core(core), _buffer(0), _screen(screen),
  _buf_offset(0)
{
  Pixel_info const *pi = core->user_state()->vstack()->canvas()->type();

  _front_txt = pi->factory->create_texture(Area(0,0), (void*)1);
  _back_txt = pi->factory->create_texture(Area(0,0), (void*)1);
  calc_label_sz(core->label_font());
}

Client_view::~Client_view()
{
  if (_screen && _screen->background() == this)
    {
      // look for other background views below
      View_stack *vs = _core->user_state()->vstack();
      // We can either search below this view in the stack, or
      // we can search from the top of the stack to find the uppermost
      // view of our session that is tagged as background
      View *v = vs->next_view(this); // search below this view
      // View *v = vs->top(); // Search from the top of the stack
      for (; v; v = vs->next_view(v))
	if (v != this && v->session() == _screen && v->background())
	  break;
      _screen->background(v);
    }

  _core->user_state()->forget_view(this);
  delete _back_txt;
  delete _front_txt;
}

inline
void
Client_view::get_info(L4Re::Video::View::Info *inf) const
{
  using L4Re::Video::Color_component;
  inf->flags = L4Re::Video::View::F_fully_dynamic;
  // we do not support changing the pixel format
  inf->flags &= ~L4Re::Video::View::F_set_pixel;
  if (above())
    inf->flags |= L4Re::Video::View::F_above;

  inf->xpos = x1();
  inf->ypos = y1();
  inf->width = w();
  inf->height = h();
  inf->buffer_offset = _buf_offset;

  Pixel_info const *pi = 0;
  pi = _front_txt->type();

  inf->bytes_per_line = pi->bytes_per_pixel() * _front_txt->size().w();
  inf->pixel_info = *pi;

  if (_buffer)
    inf->buffer_index = _buffer->index;
  else
    inf->buffer_index = ~0;
}

inline
void
Client_view::set_info(L4Re::Video::View::Info const &inf,
                      cxx::Ref_ptr<Client_buffer> const &b)
{
  Pixel_info const *pi = _core->user_state()->vstack()->canvas()->type();

  bool recalc_height = false;
  _back_txt->size(_front_txt->size());
  _back_txt->pixels(_front_txt->pixels());

  if (inf.flags & L4Re::Video::View::F_set_flags)
    set_above(inf.flags & L4Re::Video::View::F_above);

  if (inf.flags & L4Re::Video::View::F_set_background)
    {
      _core->user_state()->vstack()->push_bottom(this);
      set_as_background();
      _screen->background(this);
    }

  if (inf.has_set_bytes_per_line())
    {
      _back_txt->size(Area(inf.bytes_per_line / pi->bytes_per_pixel(), 0));
      recalc_height = true;
    }

  if (inf.has_set_buffer())
    {
      _back_txt->pixels((char *)b->local_addr() + _buf_offset);
      _buffer = b;
      recalc_height = true;
    }

  if (!_buffer)
    {
      _back_txt->size(Area(0, 0));
      _back_txt->pixels((char *)0);
      _front_txt->size(Area(0, 0));
      _front_txt->pixels((char *)0);
    }

  if (inf.has_set_buffer_offset() && _buffer)
    {
      _back_txt->pixels((char *)_buffer->local_addr() + inf.buffer_offset);
      _buf_offset = inf.buffer_offset;
      recalc_height = true;
    }

  if (recalc_height && _buffer)
    {
      unsigned long w = _back_txt->size().w();
      unsigned long bw = w * pi->bytes_per_pixel();
      unsigned long h;

      if (bw > 0 && w > 0)
	{
	  h = _buffer->size();
	  if (h > _buf_offset)
	    h -= _buf_offset;
	  else
	    h = 0;

	  h /= bw;
	}
      else
	{
	  w = 0;
	  h = 0;
	}
      _back_txt->size(Area(w, h));
    }

  if (_back_txt->size() != _front_txt->size()
      || _back_txt->pixels() != _front_txt->pixels())
    swap_textures();

  if (inf.has_set_position())
    _core->user_state()->vstack()->viewport(this, Rect(Point(inf.xpos,
            inf.ypos), Area(inf.width, inf.height)), true);
}


Mag_goos::Mag_goos(Core_api const *core)
: Icu_svr(1, &_ev_irq), _core(core)
{
  L4Re::Env const *e = L4Re::Env::env();
  _ev_ds = L4Re::Util::make_unique_cap<L4Re::Dataspace>();

  chksys(e->mem_alloc()->alloc(L4_PAGESIZE, _ev_ds.get()));
  chksys(e->rm()->attach(&_ev_ds_m, L4_PAGESIZE, L4Re::Rm::Search_addr,
                         L4::Ipc::make_cap_rw(_ev_ds.get())));

  _events = L4Re::Event_buffer(_ev_ds_m.get(), L4_PAGESIZE);
}

void Mag_client::start(Core_api *core)
{
  _core = core;
  core->registry()->register_obj(cxx::Ref_ptr<Mag_client>(this), "mag");
  if (!obj_cap().is_valid())
    printf("Service registration failed.\n");
  else
    printf("Plugin: Mag_client service started\n");
}

void Mag_goos::set_default_background(Session *_s, Property_handler const *, cxx::String const &)
{
  Mag_goos *s = static_cast<Mag_goos *>(_s);

  s->flags(F_default_background, 0);
}

namespace {
  Session::Property_handler const _opts[] =
    { { "default-background", false,  &Mag_goos::set_default_background },
      { "dfl-bg",             false,  &Mag_goos::set_default_background },
      { 0, 0, 0 }
    };
};

long
Mag_client::op_create(L4::Factory::Rights, L4::Ipc::Cap<void> &obj,
                      l4_mword_t proto, L4::Ipc::Varg_list<> const &args)
{
  if (!L4::kobject_typeid<L4Re::Console>()->has_proto(proto))
    return -L4_ENODEV;

  cxx::Ref_ptr<Mag_goos> cf(new Mag_goos(_core));
  _core->set_session_options(cf.get(), args, _opts);

  _core->register_session(cf.get());
  _core->registry()->register_obj(cf);
  cf->obj_cap()->dec_refcnt(1);

  obj = L4::Ipc::make_cap(cf->obj_cap(), L4_CAP_FPAGE_RWSD);
  return L4_EOK;
}


void
Mag_client::destroy()
{
}

inline long
Mag_goos::op_info(Goos_rights, L4Re::Video::Goos::Info &i)
{
  using L4Re::Video::Color_component;
  using L4Re::Video::Goos;

  Area a = _core->user_state()->vstack()->canvas()->size();
  Pixel_info const *mag_pi = _core->user_state()->vstack()->canvas()->type();
  i.width = a.w();
  i.height = a.h();
  i.flags = Goos::F_pointer
    | Goos::F_dynamic_views
    | Goos::F_dynamic_buffers;

  i.num_static_views = 0;
  i.num_static_buffers = 0;
  i.pixel_info = *mag_pi;

  return L4_EOK;
}

inline long
Mag_goos::op_create_buffer(Goos_rights, unsigned long size,
                           L4::Ipc::Cap<L4Re::Dataspace> &ds)
{
  cxx::Ref_ptr<Client_buffer> b(new Client_buffer(_core, size));
  _buffers.emplace_back(b);
  b->index = _buffers.size() - 1;
  ds = L4::Ipc::Cap<L4Re::Dataspace>(b->ds_cap(), L4_CAP_FPAGE_RW);
  return b->index;
}

inline long
Mag_goos::op_create_view(Goos_rights)
{
  cxx::unique_ptr<Client_view> v = cxx::make_unique<Client_view>(_core, this);
  unsigned idx = 0;
  for (View_vector::iterator i = _views.begin(); i != _views.end();
      ++i, ++idx)
    if (!*i)
      {
        *i = cxx::move(v);
        return idx;
      }

  _views.emplace_back(cxx::move(v));
  return _views.size() - 1;
}

inline long
Mag_goos::op_delete_view(Goos_rights, unsigned idx)
{
  if (idx >= _views.size())
    return -L4_ERANGE;

  _views[idx].reset(0);
  return L4_EOK;
}

inline long
Mag_goos::op_get_static_buffer(Goos_rights, unsigned idx,
                               L4::Ipc::Cap<L4Re::Dataspace> &ds)
{
  if (idx >= _buffers.size())
    return -L4_ERANGE;

  ds = L4::Ipc::Cap<L4Re::Dataspace>(_buffers[idx]->ds_cap(), L4_CAP_FPAGE_RW);
  return L4_EOK;
}

inline long
Mag_goos::op_view_info(Goos_rights, unsigned idx, L4Re::Video::View::Info &info)
{
  if (idx >= _views.size())
    return -L4_ERANGE;

  Client_view *cv = _views[idx].get();

  L4Re::Video::View::Info vi;
  vi.view_index = idx;
  cv->get_info(&info);

  return L4_EOK;
}

inline long
Mag_goos::op_set_view_info(Goos_rights, unsigned idx,
                           L4Re::Video::View::Info const &vi)
{
  if (idx >= _views.size())
    return -L4_ERANGE;

  Client_view *cv = _views[idx].get();

  cxx::Weak_ptr<Client_buffer> cb(0);
  if (vi.has_set_buffer())
    {
      if (vi.buffer_index >= _buffers.size())
	return -L4_ERANGE;

      cb = _buffers[vi.buffer_index];
    }

  cv->set_info(vi, cb);
  return L4_EOK;
}

inline long
Mag_goos::op_view_stack(Goos_rights, unsigned cvi, unsigned pvi, bool behind)
{
  Client_view *pivot = 0;
  Client_view *cv;

  if (cvi >= _views.size())
    return -L4_ERANGE;

  cv = _views[cvi].get();

  if (pvi < _views.size())
    pivot = _views[pvi].get();

  if (!pivot)
    {
      if (!behind)
	_core->user_state()->vstack()->push_bottom(cv);
      else
	_core->user_state()->vstack()->push_top(cv);
    }
  else
    _core->user_state()->vstack()->stack(cv, pivot, behind);

  return L4_EOK;
}

inline long
Mag_goos::op_view_refresh(Goos_rights, unsigned idx, int x, int y, int w, int h)
{
  if (idx >= _views.size())
    return -L4_ERANGE;

  Client_view *cv = _views[idx].get();
  _core->user_state()->vstack()->refresh_view(cv, 0, Rect(cv->p1() + Point(x,y), Area(w,h)));

  return L4_EOK;
}

inline long
Mag_goos::op_refresh(Goos_rights, int x, int y, int w, int h)
{
  _core->user_state()->vstack()->refresh_view(0, 0, Rect(Point(x,y), Area(w,h)));

  return L4_EOK;
}

void
Mag_goos::destroy()
{
  _buffers.clear();
  _views.clear();
}


Client_buffer::Client_buffer(Core_api const *, unsigned long size)
: _size(l4_round_page(size))
{
  L4Re::Rm::Unique_region<void *> dsa;
  _ds = L4Re::Util::make_unique_cap<L4Re::Dataspace>();

  L4Re::chksys(L4Re::Env::env()->mem_alloc()->alloc(_size, _ds.get()));
  L4Re::chksys(L4Re::Env::env()->rm()
                 ->attach(&dsa, _size, L4Re::Rm::Search_addr,
                          L4::Ipc::make_cap_rw(_ds.get())));

  _texture_mem = cxx::move(dsa);
}


void
Mag_goos::put_event(Hid_report *e, bool _trigger)
{
  if (post_hid_report(e, _events, Axis_xfrm_noop()) && _trigger)
    _ev_irq.trigger();
}

void
Mag_goos::put_event(l4_umword_t stream, int type, int code, int value,
                    l4_uint64_t time)
{
  L4Re::Event_buffer::Event e;
  e.time = time;
  e.payload.stream_id = stream;
  e.payload.type = type;
  e.payload.code = code;
  e.payload.value = value;
  _events.put(e);
  _ev_irq.trigger();
}


void
Client_view::draw(Canvas *c, View_stack const *, Mode mode) const
{
  Canvas::Mix_mode op = mode.flat() ? Canvas::Solid : Canvas::Mixed;
  if (mode.xray() && !mode.kill() && focused())
    op = Canvas::Solid;

  Clip_guard cg(c, *this);

  if (!c->clip_valid())
    return;

  Rgb32::Color mix_color = /*mode.kill() ? kill_color() :*/ session()->color();
  Area s(0, 0);
  if (_buffer)
    {
      c->draw_texture(_front_txt, mix_color, p1(), op);
      s = _front_txt->size();
    }

  Area r = size() - s;
  if (r.h() > 0)
    c->draw_box(Rect(p1() + Point(0, s.h()), Area(size().w(), r.h())), mix_color);

  if (r.w() > 0 && size().h() != r.h())
    c->draw_box(Rect(p1() + Point(s.w(), 0), Area(r.w(), s.h())), mix_color);
}

void
Client_view::handle_event(Hid_report *e, Point const &, bool)
{
  _screen->put_event(e, true);
}


static Mag_client _mag_client;

}}

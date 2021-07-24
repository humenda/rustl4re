#include <l4/re/env>
#include <cstdio>
#include <pthread.h>

#include <l4/re/util/debug>
#include <l4/re/event>
#include <l4/re/console>
#include <l4/re/rm>
#include <l4/re/env>
#include <l4/re/error_helper>
#include <l4/re/util/cap_alloc>
#include <l4/re/util/unique_cap>
#include <l4/re/video/goos>
#include <unistd.h>
#include <l4/scout-gfx/redraw_manager>


#include "platform.h"
#include "config.h"
#include <semaphore.h>
#include <pthread.h>
#include <cstring>
#include <l4/re/event_enums.h>
#include <l4/re/event>
#include <l4/sys/factory>
#include <l4/scout-gfx/redraw_manager>
#include <l4/scout-gfx/user_state>
#include "factory.h"

namespace {
using L4Re::Util::Unique_cap;

class Semaphore
{
private:
  sem_t _s;

public:
  Semaphore()
  { sem_init(&_s, 0, 0); }

  void up() { sem_post(&_s); }
  void down() { sem_wait(&_s); }

  ~Semaphore() { sem_destroy(&_s); }

};

class Lock
{
private:
  pthread_mutex_t _m;

public:
  Lock()
  { pthread_mutex_init(&_m, NULL); }

  ~Lock()
  { pthread_mutex_destroy(&_m); }

  void lock()
  { pthread_mutex_lock(&_m); }

  void unlock()
  { pthread_mutex_unlock(&_m); }

};

template< typename L >
class Lock_guard
{
private:
  L &_l;

public:
  explicit Lock_guard(L &l) : _l(l) { _l.lock(); }
  ~Lock_guard() { _l.unlock(); }
};

/*****************
 ** Event queue **
 *****************/
class Eventqueue
{
private:

  enum { queue_size = 1024 };

  int           _head;
  int           _tail;
  Semaphore     _sem;
  Lock  _head_lock;  /* synchronize add */

  Scout_gfx::Event _queue[queue_size];

public:

  /**
   * Constructor
   */
  Eventqueue(): _head(0), _tail(0) {}

  void add(Scout_gfx::Event const &ev)
  {
    Lock_guard<Lock> lock_guard(_head_lock);

    if ((_head + 1)%queue_size != _tail)
      {

	_queue[_head] = ev;
	_head = (_head + 1)%queue_size;
	_sem.up();
      }
  }

  void get(Scout_gfx::Event *dst_ev)
  {
    _sem.down();
    *dst_ev = _queue[_tail];
    _tail = (_tail + 1)%queue_size;
  }

  int pending() { return _head != _tail; }

} _evqueue;

/******************
 ** Timer thread **
 ******************/

using Mag_gfx::Point;

class Timer_thread
{
private:

  pthread_t _th;
  Unique_cap<L4Re::Dataspace> _ev_ds;
  Unique_cap<L4::Irq> _ev_irq;
  L4Re::Rm::Unique_region<void *> _ev_ds_addr;
  L4Re::Event_buffer _evb;

  Point _m;
  int _key_cnt;

  l4_uint64_t _timer_tick;

  void _set_ev(Scout_gfx::Event &ev)
  {
    ev.key_cnt = _key_cnt;
    ev.m = _m;
  }

  void _import_events()
  {
    using Scout_gfx::Event;
    typedef L4Re::Event_buffer::Event Re_ev;

    for (Re_ev *re = _evb.next(); re; re->free(), re = _evb.next())
      {
	Event ev;
	Point om = _m;
	switch (re->payload.type)
	  {
	  case L4RE_EV_KEY:
	    switch (re->payload.value)
	      {
	      case 1:  ++_key_cnt; ev.type = Event::PRESS; break;
	      case 0:  --_key_cnt; ev.type = Event::RELEASE; break;
	      default: continue;
	      }
	    if (_key_cnt < 0)
	      _key_cnt = 0;

	    _set_ev(ev);
	    ev.code = re->payload.code;
	    ev.wx = 0;
	    ev.wy = 0;
	    break;

	  case L4RE_EV_ABS:
	    switch (re->payload.code)
	      {
	      case L4RE_ABS_X:
		_m = Point(re->payload.value, _m.y());
		break;
	      case L4RE_ABS_Y:
		_m = Point(_m.x(), re->payload.value);
		break;
	      default: continue;
	      }
	    if (_m != om)
	      {
		_set_ev(ev);
		ev.wx = 0;
		ev.wy = 0;
		ev.type = Event::MOTION;
		ev.code = 0;
		break;
	      }
	    continue;

	  case L4RE_EV_REL:
	    _set_ev(ev);
	    ev.type = Event::WHEEL;
	    ev.code = 0;
	    switch (re->payload.code)
	      {
	      case L4RE_REL_WHEEL:
		ev.wx = 0;
		ev.wy = re->payload.value;
		break;
	      case L4RE_REL_HWHEEL:
		ev.wy = 0;
		ev.wx = re->payload.value;
		break;
	      default:
		continue;
	      }
	    break;

	  default:
	    continue;
	  }

	_evqueue.add(ev);
      }
  }

  static void *_tstart(void *t)
  {
    reinterpret_cast<Timer_thread*>(t)->entry();
    return NULL;
  }

public:

  l4_uint64_t ticks() const { return _timer_tick; }

  /**
   * Constructor
   *
   * Start thread immediately on construction.
   */
  Timer_thread()
    : _key_cnt(0), _timer_tick(0)
  {}

  void start(L4::Cap<L4Re::Event> const &e)
  {

    using L4Re::Util::make_unique_cap;
    using L4Re::chksys;

    _ev_ds = make_unique_cap<L4Re::Dataspace>();
    _ev_irq = make_unique_cap<L4::Irq>();
    chksys(L4Re::Env::env()->factory()->create(_ev_irq.get()));
    chksys(e->get_buffer(_ev_ds.get()));
    chksys(L4Re::Env::env()->rm()->attach(&_ev_ds_addr, _ev_ds->size(),
           L4Re::Rm::F::Search_addr | L4Re::Rm::F::RW, _ev_ds.get(), 0, L4_PAGESHIFT));

    _evb = L4Re::Event_buffer(_ev_ds_addr.get(), _ev_ds->size());

    pthread_create(&_th, NULL, &_tstart, this);
  }

  void entry()
  {
    while (1)
      {
	Scout_gfx::Event ev;
	ev.assign(Scout_gfx::Event::TIMER, _m, 0);
	_evqueue.add(ev);

	_import_events();

	usleep(10 * 1000);
	_timer_tick += 10;
      }
  }
};



class Re_view : public Scout_gfx::View
{
private:
  Rect _v;
  Scout_gfx::Redraw_manager *_rdm;
  Mag_gfx::Canvas *_canvas;
  L4Re::Video::View _view;
  L4Re::Video::View::Info _view_info;

  unsigned long _buffer_offset;

public:

  Re_view(Rect const &r, Rect const &ol, Scout_gfx::Redraw_manager *rdm,
          Mag_gfx::Canvas *canvas, L4Re::Video::View const &v,
          unsigned long buffer_offset);
  /**
   * View geometry accessor functions
   */
  virtual Rect geometry() const { return _v; }

  virtual void redraw(Rect const &r)
  { _rdm->request(r-_v.p1()); }

  void scr_refresh(Rect const &r)
  { _view.refresh(r.x1(), r.y1(), r.w(), r.h()); }

  void set_buf_pos(unsigned long offset)
  {
    _buffer_offset = offset;
    _view.set_viewport(_v.x1(), _v.y1(), _v.w(), _v.h(), offset);
  }
  /**
   * Define geometry of viewport on screen
   *
   * The specified area is relative to the screen
   * of the platform.
   */
  virtual Rect set_geometry(Rect const &pos, bool do_redraw)
  {
    // printf("view geometry: %d,%d-%d,%d %d\n", pos.x1(), pos.y1(), pos.x2(), pos.y2(), do_redraw);
    if (!_view_info.has_set_position())
      return _v;

    Rect old_pos = _v;
    _v = pos; //Rect(Point(0,0), scr_size());
    if (pos != old_pos)
      {
	//_rdm->request(old_pos);
	_view.set_viewport(_v.x1(), _v.y1(), _v.w(), _v.h(), _buffer_offset);
	if (do_redraw)
	  _rdm->request(pos);
      }

    return _v;
  }

  /**
   * Bring Scouts view ontop
   */
  virtual void top() { _view.push_top(); }

  void set_view_info(L4Re::Video::View::Info const &vi)
  { _view_info = vi; }

  L4Re::Video::View::Info const *view_info() const
  { return &_view_info; }

  Mag_gfx::Pixel_info const *pixel_info() const
  { return _canvas->type(); }
};

Re_view::Re_view(Rect const &r, Rect const &ol, Scout_gfx::Redraw_manager *rdm,
                 Mag_gfx::Canvas *canvas, L4Re::Video::View const &v, unsigned long buffer_offset)
: _v(r & ol), _rdm(rdm), _canvas(canvas), _view(v), _buffer_offset(buffer_offset)
{
}

class Re_ust : public Scout_gfx::User_state
{
public:
  void draw(Mag_gfx::Canvas *c, Mag_gfx::Point const &p)
  {
    for (Scout_gfx::Widget *e = _first; e; e = e->next)
      e->try_draw(c, p);
  }
};

class Re_pf : public Platform, public Scout_gfx::Screen_update
{
private:

  L4Re::Video::Goos::Info _scr_info;
  L4::Cap<L4Re::Console> _screen;
  Unique_cap<L4Re::Dataspace> _fb_ds;
  L4Re::Rm::Unique_region<char *> _fb_addr;
  Area _size;

  bool _ready;

  Re_view *_view;
  Timer_thread _timer;
  Scout_gfx::Redraw_manager _rdm;
  Re_ust _ust;
  Mag_gfx::Canvas *_canvas;

  void *_front_buffer;
  void *_back_buffer;
  bool _dbl_buffer_2;

public:
  static bool probe();
  Re_pf(Rect const &sz);
  Mag_gfx::Pixel_info const *pixel_info() const
  { return _canvas->type(); }

  int max_num_views() { return 1; }
  Scout_gfx::View *create_view(Rect const &/*r*/)
  {
    return _view;
  }

  virtual int initialized()
  { return _ready; }

  /**
   * Request screen width and height
   */
  virtual Area scr_size() { return _size; }



  /**
   * Get timer ticks in miilliseconds
   */
  virtual unsigned long timer_ticks() { return _timer.ticks(); } 

  /**
   * Request if an event is pending
   *
   * \retval 1  event is pending
   * \retval 0  no event pending
   */
  virtual int event_pending() { return _evqueue.pending(); }

  /**
   * Request event
   *
   * \param e  destination where to store event information.
   *
   * If there is no event pending, this function blocks
   * until there is an event to deliver.
   */
  virtual int get_event(Scout_gfx::Event *out_e)
  {
    _evqueue.get(out_e);
    return 1;
  }

  /**
   * Screen update interface
   */
  virtual void *scr_adr()
  { return _front_buffer; }
#if 0
    long ofs = _size.h() * _dbl_buffer_2 * _view_info.bytes_per_line;
    return _fb_addr.get() + _view_info.buffer_offset + ofs;
  }
#endif
  virtual void  scr_update(Rect const &r)
  {
    _view->scr_refresh(r);
  }

  /**
   * Exchange foreground and back buffers
   */
  void flip_buf_scr()
  {
    if (_view->view_info()->has_static_buffer_offset())
      {
	copy_buf_to_scr(Rect(_size));
	return;
      }
    _dbl_buffer_2 = !_dbl_buffer_2;
    void *tmp = _front_buffer;
    _front_buffer = _back_buffer;
    _back_buffer = tmp;
    _view->set_buf_pos(_size.h() * (_dbl_buffer_2) * _view->view_info()->bytes_per_line);
    _canvas->buffer(_back_buffer);
  }

  void copy_buf_to_scr(Rect const &r);

  void process_redraws()
  { _rdm.process(_view->geometry()); }

  Scout_gfx::Parent_widget *root() { return &_ust; }
};

bool
Re_pf::probe()
{
  L4Re::Env const *e = L4Re::Env::env();
  L4::Cap<void> _mag = e->get_cap<void>("fb");
  return _mag.is_valid();
}

Re_pf::Re_pf(Rect const &sz)
: Platform(), _size(sz.area()), _ready(false), _view(0),
  _canvas(0), _dbl_buffer_2(false)
{
  using L4Re::chksys;
  using L4Re::Util::make_unique_cap;
  (void)sz;

  L4Re::Video::View::Info _view_info;

  L4Re::Env const *e = L4Re::Env::env();
  _screen = chkcap(e->get_cap<L4Re::Console>("fb"));
  chksys(_screen->L4Re::Video::Goos::info(&_scr_info), "requesting screen info");

  L4Re::Video::View view;

  if (_scr_info.has_dynamic_views())
    chksys(_screen->create_view(&view), "creating new view");
  else
    view = _screen->view(0);

  chksys(view.info(&_view_info), "requesting view infos");
#if 0
  printf("SCREEN: size = %ld, %ld, bypp=%d\n",
         _scr_info.width, _scr_info.height,
         _scr_info.pixel_info.bytes_per_pixel());
#endif

  _fb_ds = make_unique_cap<L4Re::Dataspace>();

  if (!_view_info.has_static_buffer())
    {
      _size = sz.area();
      unsigned long bsize = _size.pixels() * _scr_info.pixel_info.bytes_per_pixel() * 2;
      _view_info.buffer_index = chksys(_screen->create_buffer(bsize, _fb_ds.get()));
      _view_info.xpos = 0;
      _view_info.ypos = 0;
      _view_info.width = sz.w();
      _view_info.height = sz.h();
      _view_info.buffer_offset = 0;
      _view_info.bytes_per_line = _size.w() * _scr_info.pixel_info.bytes_per_pixel();
      chksys(view.set_info(_view_info), "setting view infos");
//      view.push_top();
      _back_buffer = 0;

      Config::browser_attr      = 7;
    }
  else
    {
      chksys(_screen->get_static_buffer(_view_info.buffer_index, _fb_ds.get()), "requesting static bvuffer");
      _size = Area(_view_info.width, _view_info.height);
      _back_buffer = new char[_fb_ds->size()];
    }



  chksys(e->rm()->attach(&_fb_addr, _fb_ds->size(),
                         L4Re::Rm::F::Search_addr | L4Re::Rm::F::RW,
                         _fb_ds.get(), 0, L4_SUPERPAGESHIFT));

  L4Re::Util::Dbg dbg;
  _view_info.dump(dbg);
  dbg.printf("  memory %p-%p\n", (void*)_fb_addr.get(),
             (void*)(_fb_addr.get() + _fb_ds->size()));

  _timer.start(_screen);

  _max = scr_size();

  _ust.set_geometry(Rect(Point(0,0), Area(_scr_info.width, _scr_info.height)));
  ::Factory *f = dynamic_cast< ::Factory* >(::Factory::set.find(_view_info.pixel_info));

  if (!f)
    {
      printf("No factory for this framebuffer config\n");
      return;
    }

  _front_buffer = _fb_addr.get() + _view_info.buffer_offset;
  if (!_back_buffer)
    _back_buffer = _fb_addr.get() + _view_info.buffer_offset + _size.h() * _view_info.bytes_per_line;

  _canvas = f->create_canvas(_back_buffer, scr_size(), _view_info.bytes_per_line);
  if (!_canvas)
    {
      printf("Could not create canvas\n");
      return;
    }

  _view = new Re_view(Rect(sz.p1(), _size), Rect(Point(0,0), scr_size()), &_rdm, _canvas, view,
                      _view_info.buffer_offset);

  _view->set_view_info(_view_info);

  _rdm.setup(_canvas, this, scr_size());
  _rdm.root(&_ust);

  _ready = true;
}

void
Re_pf::copy_buf_to_scr(Rect const &_r)
{
  unsigned bpp = _view->view_info()->pixel_info.bytes_per_pixel();
  Rect r = _r & Rect(scr_size());// - o;
  /* copy background buffer to foreground buffer */
  int len     = r.w() * bpp;
  int linelen = _view->view_info()->bytes_per_line;

  char *src = (char*)_back_buffer + r.y1() * linelen + r.x1() * bpp;
  char *dst = (char*)_front_buffer + r.y1() * linelen + r.x1() * bpp;

  //	blit(src, linelen, dst, linelen, len, h);
  for (int j = 0; j < r.h(); j++, dst += linelen, src += linelen)
    memcpy(dst, src, len);
}

static Platform::Pf_factory_t<Re_pf> pff;

}



#include <l4/scout-gfx/simple_window_deco>
#include <l4/scout-gfx/icon>
#include <l4/scout-gfx/factory>

#define SIZER_RGBA    _binary_sizer_rgba_start
extern unsigned char SIZER_RGBA[];


namespace Scout_gfx {

namespace {

class Deco_widget : public Parent_widget
{
public:
  enum  { th = 16 };
  Icon *_sizer;

  Sizer_event_handler _sevh;
  Mover_event_handler _mevh;

  Deco_widget(Factory *f, Window *w)
  : _sevh(w), _mevh(w)
  {
    _sizer = f->create_icon(SIZER_RGBA, Area(32, 32));
    _sizer->alpha(100);
    _sizer->event_handler(&_sevh);
    append(_sizer);
  }

  Mag_gfx::Area preferred_size() const { return Mag_gfx::Area(0,0); }
  Mag_gfx::Area min_size() const  { return Mag_gfx::Area(0,0); }
  Mag_gfx::Area max_size() const  { return Mag_gfx::Area(Mag_gfx::Area::Max_w, Mag_gfx::Area::Max_h); }
  Mag_gfx::Orientations expanding() const { return Mag_gfx::Horz; }
  bool empty() const { return false; }

  void draw(Mag_gfx::Canvas *c, Mag_gfx::Point const &p)
  {
    using namespace Mag_gfx;
    static const Mag_gfx::Rgba32::Color grey(42, 62, 62);
    static const Mag_gfx::Rgba32::Color black(0, 0, 0);

    c->draw_box(Rect(p, Area(size().w(), th)), grey);

    c->draw_box(Rect(p + Point(0, size().h() - 1), Area(size().w(), 1)), black);
    Area hl(1, size().h() - th);
    c->draw_box(Rect(p + Point(0, th), hl), black);
    c->draw_box(Rect(p + Point(size().w() - 1, th), hl), black);

    Parent_widget::draw(c, p);

  //  _sizer->draw(c, p + Point(size() - _sizer->size()));
  }

  Rect title_rect() const
  {
    Rect r = geometry();
    r = Rect(r.p1(), Area(r.w(), th));
    return r;
  }

  Widget *find(Point const &p)
  {
     if (!findable() || !geometry().contains(p))
       return 0;

     if (title_rect().contains(p))
       return this;

     return _sizer->find(p - pos());
  }

  Widget *handle_event(Event const &e)
  {
    if (!visible())
      return 0;

    _mevh.handle(e);
    return this;
  }

  void set_geometry(Rect const &r)
  {
    Widget::set_geometry(r);
    _sizer->set_geometry(Rect(Point(size() - _sizer->size()), _sizer->size()));
  }
};

}

Widget *
Simple_window_deco_policy::create_deco(Window *w) const
{
  return new Deco_widget(_f, w);
}

void
Simple_window_deco_policy::set_deco_mode(Window::Mode mode, Widget *deco,
                                         Insets *insets)
{
  if (mode == Window::Fullscreen)
    {
      insets->tl = Area(0, 0);
      insets->br = Area(0, 0);
      deco->findable(false);
      deco->visible(false);
    }
  else
    {
      insets->tl = Area(1, 16);
      insets->br = Area(1, 1);
      deco->findable(true);
      deco->visible(true);
    }
}

}

#include <l4/scout-gfx/window>

#include <l4/re/event_enums.h>
namespace Scout_gfx {

using namespace Mag_gfx;

namespace {

class Win_layout : public Layout
{
private:
  Window::Deco_policy::Insets _inset;

  Layout_item *_child;

public:
  Win_layout(Layout_item *child, Window::Deco_policy::Insets **insets)
  : _inset(), _child(child)
  {
    _inset.tl = Area(0,0);
    _inset.br = Area(0,0);
    _child->set_parent_layout_item(this);
    *insets = &_inset;
  }

  virtual ~Win_layout() {}

  Area preferred_size() const
  { return _child->preferred_size() + _inset.tl + _inset.br; }

  Area min_size() const
  { return _child->min_size() + _inset.tl + _inset.br; }

  Area max_size() const
  { return _child->max_size() + _inset.tl + _inset.br; }

  Orientations expanding() const { return _child->expanding(); }
  bool empty() const  { return false; }
  bool has_height_for_width() const
  { return _child->has_height_for_width(); }

  int height_for_width(int w) const
  { return _child->height_for_width(w); }

  int min_height_for_width(int w) const
  { return _child->min_height_for_width(w); }

  //void child_invalidate() { _child->invalidate(); }
  //void invalidate() { _child->invalidate(); }

  void set_geometry(Rect const &r)
  {
    Rect x
      = r.offset(_inset.tl.w(), _inset.tl.h(), -_inset.br.w(), -_inset.br.h());

    _child->set_geometry(x);
  }

  Rect geometry() const
  {
    return _child->geometry().offset(-_inset.tl.w(), -_inset.tl.h(),
                                     _inset.br.w(), _inset.br.h());
  }

  void add_item(Layout_item *) {}
  Layout_item *remove_item(int) { return 0; }
  Layout_item *item(int) const { return _child; }
};

}

Window::Window(Window::Deco_policy *dp, View *v,
               Area const &max_size)
: Basic_window(v, max_size),
  // this is some stupid hack, needs to be defined by the app
  _normal_pos(Point(100, 100), Area(300, 200)),
  _deco_policy(dp)
{
  _deco = _deco_policy->create_deco(this);
  Basic_window::set_child_layout(new Win_layout(&_content, &_insets));
  _deco_policy->set_deco_mode(Window::Fullscreen, _deco, _insets);

  Basic_window::append(&_content);
  Basic_window::append(_deco);
}


void
Window::set_window_mode(Mode mode)
{
  _deco_policy->set_deco_mode(mode, _deco, _insets);
  if (mode == Normal)
    set_geometry(_normal_pos);
  else
    {
      _normal_pos = geometry();
      switch (mode)
	{
	case Fullscreen:
	case Maximized:
	  set_geometry(Rect(Point(0,0), max_size()));
	  break;
	case Minimized:
	  set_geometry(Rect(pos(), min_size()));
	  break;
	default:
	  break;
	}
    }
  redraw_area(Rect(Point(0, 0), size()));
}

}

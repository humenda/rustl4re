#include "browser_window.h"
#include "factory.h"

#include <l4/scout-gfx/sky_texture>
#include <l4/scout-gfx/stack_layout>
#include <l4/scout-gfx/box_layout>
#include <l4/scout-gfx/scroll_pane>
#include <l4/scout-gfx/fonts>


/****************************
 ** External graphics data **
 ****************************/

#define SIZER_RGBA    _binary_sizer_rgba_start
#define TITLEBAR_RGBA _binary_titlebar_rgba_start

extern unsigned char SIZER_RGBA[];
extern unsigned char TITLEBAR_RGBA[];

static Mag_gfx::Font &title_font = Scout_gfx::Fonts::vera18;

class Browser_sizer_event_handler : public Scout_gfx::Sizer_event_handler
{
private:

  Browser_window *_browser_win;
  Scout_gfx::Widget              *_ca;         /* original visible element     */

  /**
   * Event handler interface
   */
  void start_drag()
  {
    Sizer_event_handler::start_drag();
    _ca = _browser_win->curr_anchor();
  }

  void do_drag()
  {
    Sizer_event_handler::do_drag();
    _browser_win->go_to(_ca, 0);
  }

public:

  /**
   * Constructor
   */
  Browser_sizer_event_handler(Browser_window *browser_win)
  : Sizer_event_handler(browser_win)
  {
    _browser_win = browser_win;
  }
};


/******************************
 ** Browser window interface **
 ******************************/

Browser_window::Browser_window(Factory *f,
    Scout_gfx::Document *initial_content,
    Scout_gfx::View *pf,
    Area const &max_sz, int attr)
: Basic_window(pf, max_sz),
  _titlebar(f, &title_font), _cont(0)
{
  using namespace Mag_gfx;
  using namespace Scout_gfx;
  /* init attributes */
  _document = initial_content;
  _attr     = attr;

  Scout_gfx::Event_handler *mev = new Scout_gfx::Mover_event_handler(this);
  _panel = f->create_panel(this);
  _panel->event_handler(mev);

  Scout_gfx::Sky_texture *txt = f->create_sky();
  txt->generate(Area(512, 512));

  Scout_gfx::Scroll_pane *sp = new Scout_gfx::Scroll_pane(f);
  sp->set_texture(txt);
  append(sp);

  /* init docview and history with initial document */
  //_docview.texture(_texture);
  //_docview.voffset(doc_offset());
  //voffset(doc_offset());
  _history.add(initial_content);


  /* titlebar */
  _titlebar.rgba(TITLEBAR_RGBA);
  _titlebar.text(_document->title);
  _titlebar.event_handler(mev);

  _min_sz = _panel->size() + Area(0, 250);
  _size = _min_sz;

  /* adopt widgets as child elements */
  append(_panel);

  _shadow = f->create_shadow();
  append(_shadow);

  /* resize handle */
  if (_attr & ATTR_SIZER)
    {
      _sizer = f->create_icon(SIZER_RGBA, Area(32, 32));
      _sizer->event_handler(new Browser_sizer_event_handler(this));
      _sizer->alpha(100);
      sp->set_reserved_scrollbar_area(_sizer->min_size());
      append(_sizer);
    }
  else
    _sizer = 0;

  if (_attr & ATTR_TITLEBAR) append(&_titlebar);

  Scout_gfx::Layout *l = Scout_gfx::Stack_layout::create();
  set_child_layout(l);

  Scout_gfx::Layout *n = Scout_gfx::Box_layout::create_vert();
  l->add_item(n);

  n->set_alignment(Align_top);
  if (_attr & ATTR_TITLEBAR)
    n->add_item(&_titlebar);

  n->add_item(_panel);
  n->add_item(_shadow);
  l->add_item(sp);
  n->add_item(sp->view_pane_layout_item());
  Scout_gfx::Layout *vl1 = Scout_gfx::Box_layout::create_vert();
  vl1->set_margin(5);
  sp->set_child_layout(vl1);
  _cont_pane = sp;

  Scout_gfx::Layout *n2 = Scout_gfx::Box_layout::create_horz();
  n2->set_alignment(Align_bottom | Align_right);
  if (_sizer)
    n2->add_item(_sizer);
  else
    n2->add_item(new Scout_gfx::Spacer_item(Mag_gfx::Horizontal));
  l->add_item(n2);
  _content(initial_content);

}


/***********************
 ** Browser interface **
 ***********************/

Scout_gfx::Parent_widget *Browser_window::_content()
{
  return _cont;
}


void Browser_window::_content(Parent_widget *content)
{
  if (!content || (content == _cont))
    return;

  content->fill_cache(view()->pixel_info());
  if (_cont)
    {
      _cont_pane->child_layout()->remove_item(_cont);
      _cont_pane->remove(_cont);
    }
  _cont = content;
  _cont_pane->child_layout()->add_item(_cont);
  _cont_pane->append(_cont);
  _cont_pane->refresh_geometry();
  refresh();
}


Widget *Browser_window::curr_anchor()
{ return _cont_pane->find(_cont_pane->child_layout()->geometry().p1() + Point (5, 5)); }



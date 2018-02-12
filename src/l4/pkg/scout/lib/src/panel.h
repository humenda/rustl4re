#pragma once

#include "browser_if.h"
#include "refracted_icon.h"

#include <l4/scout-gfx/box_layout>
#include <l4/scout-gfx/stack_layout>


class Factory;


#define IOR_MAP       _binary_ior_map_start
#define HOME_RGBA     _binary_home_rgba_start
#define COVER_RGBA    _binary_cover_rgba_start
#define INDEX_RGBA    _binary_index_rgba_start
#define ABOUT_RGBA    _binary_about_rgba_start
#define FORWARD_RGBA  _binary_forward_rgba_start
#define BACKWARD_RGBA _binary_backward_rgba_start

extern short         IOR_MAP[];
extern unsigned char HOME_RGBA[];
extern unsigned char COVER_RGBA[];
extern unsigned char INDEX_RGBA[];
extern unsigned char ABOUT_RGBA[];
extern unsigned char FORWARD_RGBA[];
extern unsigned char BACKWARD_RGBA[];

/* icon graphics data */
static unsigned char *glow_icon_gfx[] = {
    HOME_RGBA,
    BACKWARD_RGBA,
    FORWARD_RGBA,
    INDEX_RGBA,
    ABOUT_RGBA,
};

/* color definitions for glowing effect of the icons */
static Scout_gfx::Color glow_icon_col[] = {
    Scout_gfx::Color(210, 210,   0),
    Scout_gfx::Color(  0,   0, 160),
    Scout_gfx::Color(  0,   0, 160),
    Scout_gfx::Color(  0, 160,   0),
    Scout_gfx::Color(160,   0,   0),
};

/**
 * Transform rgba source image to image with native pixel type
 *
 * If we specify an empty buffer as alpha channel (all values zero), we simply
 * assign the source image data to the destination buffer. If there are valid
 * values in the destination alpha channel, we paint the source image on top
 * of the already present image data. This enables us to combine multiple
 * rgba buffers (layers) into one destination buffer.
 */
template <typename PTx>
static void extract_rgba(const unsigned char *_src, int w, int h,
    typename PTx::Pixel *dst_pixel, unsigned char *dst_alpha)
{
  using namespace Mag_gfx;
  typedef typename PTx::Color Color;

  Rgba32::Pixel const *src = reinterpret_cast<Rgba32::Pixel const*>(_src);

  for (int i = 0; i < w*h; ++i, ++src)
    {
      Rgba32::Color const s = *src;
      if (dst_alpha[i])
	{
	  dst_pixel[i] = PTx::mix(dst_pixel[i], color_conv<Color>(s), s.a());
	  dst_alpha[i] = std::max((int)dst_alpha[i], s.a());
	}
      else
	{
	  dst_pixel[i] = color_conv<Color>(s);
	  dst_alpha[i] = s.a();
	}
    }
}


template< typename PT >
class Panel : public Scout_gfx::Parent_widget
{
  enum
  {
    ICON_HOME     = 0,
    ICON_BACKWARD = 1,
    ICON_FORWARD  = 2,
    ICON_INDEX    = 3,
    ICON_ABOUT    = 4
  };

  /**
   * Constants
   */
  enum
  {
    _NUM_ICONS = 5,     /* number of icons          */
    _IW        = 64,    /* browser icon width       */
    _IH        = 64,    /* browser icon height      */
    _PANEL_W   = 320,   /* panel tile width         */
    _PANEL_H   = _IH,   /* panel tile height        */
    _SB_XPAD   = 5,     /* hor. pad of scrollbar    */
    _SB_YPAD   = 10,    /* vert. pad of scrollbar   */
    _SCRATCH   = 7      /* scratching factor        */
  };
protected:
  typedef typename PT::Pixel Pixel;
  Area preferred_size() const
  {
    if (child_layout())
      return child_layout()->preferred_size();

    return Area(0, 0);
  }

  Area min_size() const
  {
    if (child_layout())
      return child_layout()->min_size();

    return Area(0, 0);
  }

  Area max_size() const
  {
    if (child_layout())
      return child_layout()->max_size();

    return Area(Area::Max_w, Area::Max_h);
  }

  Mag_gfx::Orientations expanding() const { return Mag_gfx::Horizontal; }
  bool empty() const { return false; }

  void set_geometry(Rect const &r)
  {
    if (child_layout())
      child_layout()->set_geometry(Rect(r.area()));

    _pos = r.p1();
    _size = min_size().max(r.area()).min(max_size());
  }

  Rect geometry() const { return Rect(_pos, _size); }

  /**
   * Widgets
   */
  Pixel                      _icon_fg        [_NUM_ICONS][_IH][_IW];
  unsigned char              _icon_fg_alpha  [_NUM_ICONS][_IH][_IW];
  Refracted_icon<PT, short>  _icon           [_NUM_ICONS];
  Pixel                      _icon_backbuf   [_IH*2][_IW*2];
  Pixel                      _panel_fg       [_PANEL_H][_PANEL_W];
  unsigned char              _panel_fg_alpha [_PANEL_H][_PANEL_W];
  short                      _panel_distmap  [_PANEL_H*2][_PANEL_W*2];
  Refracted_icon<PT, short>  _panel;
  Pixel                      _panel_backbuf  [_PANEL_H*2][_PANEL_W*2];
  Scout_gfx::Icon           *_glow_icon[_NUM_ICONS];

  class Ev_proxy : public Scout_gfx::Event_handler
  {
  private:
    Widget *b;

  public:
    Ev_proxy(Widget *b) : b(b) {}
    /**
     * Event handler interface
     */
    bool handle(Scout_gfx::Event const &ev)
    { return b->handle_event(ev); }
  };

  Ev_proxy _ev_proxy;

  void handler(Widget *e, Browser_if *b, bool (Browser_if::*f)());

public:
  Panel(Scout_gfx::Factory *f, Browser_if *);

};


/********************
 ** Event handlers **
 ********************/

class Iconbar_event_handler : public Scout_gfx::Event_handler
{
private:

  Scout_gfx::Fader *_fader;
  Browser_if *_b;
  bool (Browser_if::*_f)();

public:

  /**
   * Constructor
   */
  Iconbar_event_handler(Scout_gfx::Fader *fader, Browser_if *b,
                        bool (Browser_if::*f)())
  : _fader(fader), _b(b), _f(f) {}

  /**
   * Event handler interface
   */
  bool handle(Scout_gfx::Event const &ev)
  {
    /* start movement with zero speed */
    if ((ev.type != Scout_gfx::Event::PRESS) || (ev.key_cnt != 1))
      return true;

    /* no flashing by default */
    int flash = (_b->*_f)();

    /* flash clicked icon */
    if (0 && flash)
      {

	/* flash fader to the max */
	_fader->step(4);
	_fader->curr(190);
      }

    return true;
  }
};

template< typename PT >
void
Panel<PT>::handler(Widget *e, Browser_if *b, bool (Browser_if::*f)())
{
  e->event_handler(new Iconbar_event_handler(dynamic_cast<Scout_gfx::Fader*>(e), b, f));
}

template< typename PT >
Panel<PT>::Panel(Scout_gfx::Factory *f, Browser_if *b)
: _ev_proxy(this)
{
  using namespace Mag_gfx;
  using namespace Scout_gfx;


  _size = Area(_NUM_ICONS*_IW, _IH);

  /* init icons */
  memset(_icon_fg,       0, sizeof(_icon_fg));
  memset(_icon_fg_alpha, 0, sizeof(_icon_fg_alpha));
  for (int i = 0; i < _NUM_ICONS; i++)
    {
      /* convert rgba raw image to PT pixel format and alpha channel */
      extract_rgba<PT>(COVER_RGBA, _IW, _IH,
	  _icon_fg[i][0], _icon_fg_alpha[i][0]);

      /* assign back buffer, foreground and distmap to icon */
      _icon[i].backbuf(_icon_backbuf[0], 1);
      _icon[i].distmap(IOR_MAP, _IW*2, _IH*2);
      _icon[i]._min_size = Area(_IW, _IH);
      _icon[i].foreground(_icon_fg[i][0], _icon_fg_alpha[i][0]);

      /* apply foreground graphics to icon */
      extract_rgba<PT>(glow_icon_gfx[i], _IW, _IH,
	  _icon_fg[i][0], _icon_fg_alpha[i][0]);

      _icon[i].event_handler(&_ev_proxy);

      /* init glow icon */
      Scout_gfx::Icon *fadeicon = f->create_icon();

      _glow_icon[i] = fadeicon;
      fadeicon->glow(glow_icon_gfx[i], Area(_IW, _IH), glow_icon_col[i]);
      fadeicon->default_alpha(0);
      fadeicon->focus_alpha(100);
      fadeicon->alpha(0);
    }

  handler(_glow_icon[ICON_HOME], b, &Browser_if::go_home);
  handler(_glow_icon[ICON_FORWARD], b, &Browser_if::go_forward);
  handler(_glow_icon[ICON_BACKWARD], b, &Browser_if::go_backward);
  handler(_glow_icon[ICON_INDEX], b, &Browser_if::go_toc);
  handler(_glow_icon[ICON_ABOUT], b, &Browser_if::go_about);

  /*
   * All icons share the same distmap. Therefore we need to scratch
   * only one distmap to affect all icons.
   */
  _icon[0].scratch(_SCRATCH);

  /* create panel tile texture */
  /*
   * NOTE: The panel height must be the same as the icon height.
   */
  for (int j = 0; j < _PANEL_H; j++)
    for (int i = 0; i < _PANEL_W; i++) {
	_panel_fg       [j][i] = _icon_fg       [ICON_INDEX][j][i&0x1];
	_panel_fg_alpha [j][i] = _icon_fg_alpha [ICON_INDEX][j][i&0x1] + random()%3;
    }

  /* init panel background */
  _panel.backbuf(&_panel_backbuf[0][0]);
  _panel.distmap(_panel_distmap[0], _PANEL_W*2, _PANEL_H*2);
  _panel.foreground(_panel_fg[0], _panel_fg_alpha[0]);
  _panel.scratch(_SCRATCH);
  _panel.event_handler(&_ev_proxy);
  _panel._min_size = Area(_IW, _IH);

  Scout_gfx::Layout *l = Scout_gfx::Box_layout::create_horz();
  set_child_layout(l);
  for (int i = 0; i <= ICON_INDEX; i++) {
      Scout_gfx::Layout *sl = Scout_gfx::Stack_layout::create();
      sl->add_item(&_icon[i]);
      sl->add_item(_glow_icon[i]);
      l->add_item(sl);
      append(&_icon[i]);
      append(_glow_icon[i]);
  }
  append(&_panel);
  l->add_item(&_panel);
  Scout_gfx::Layout *sl = Scout_gfx::Stack_layout::create();
  sl->add_item(&_icon[ICON_ABOUT]);
  sl->add_item(_glow_icon[ICON_ABOUT]);
  l->add_item(sl);
  append(&_icon[ICON_ABOUT]);
  append(_glow_icon[ICON_ABOUT]);
}



/*
 * \brief   Browser window interface
 * \date    2005-10-24
 * \author  Norman Feske <norman.feske@genode-labs.com>
 */

/*
 * Copyright (C) 2005-2009
 * Genode Labs, Feske & Helmuth Systementwicklung GbR
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#pragma once

#include <l4/scout-gfx/scroll_pane>
#include <l4/scout-gfx/horizontal_shadow>
#include "platform.h"
#include <l4/scout-gfx/redraw_manager>
#include "browser.h"
#include <l4/scout-gfx/basic_window>
#include <l4/scout-gfx/titlebar>

class Factory;

class Browser_window
: public Scout_browser,
  public Scout_gfx::Basic_window
{

private:

  /**
   * Constants
   */
  enum
  {
    _TH        = 32,    /* height of title bar      */
    _SB_XPAD   = 5,     /* hor. pad of scrollbar    */
    _SB_YPAD   = 10,    /* vert. pad of scrollbar   */
  };

  /**
   * General properties
   */
  int  _attr;   /* attribute mask */

  /**
   * Widgets
   */
  Scout_gfx::Titlebar   _titlebar;
  Scout_gfx::Widget    *_texture;
  Scout_gfx::Widget    *_shadow;
  Scout_gfx::Parent_widget  *_cont;
  Scout_gfx::Scroll_pane *_cont_pane;
  Scout_gfx::Icon      *_sizer;
  Scout_gfx::Parent_widget    *_panel;

protected:

  /**
   * Browser interface
   */
  void _content(Scout_gfx::Parent_widget *content);
  Scout_gfx::Parent_widget *_content();

public:

  /**
   * Browser window attributes
   */
  enum
  {
    ATTR_SIZER    = 0x1,  /* browser window has resize handle  */
    ATTR_TITLEBAR = 0x2,  /* browser window has titlebar       */
    ATTR_BORDER   = 0x4,  /* draw black outline around browser */
  };

  /**
   * Constructor
   *
   * \param scr_adr   base address of screen buffer
   * \param scr_w     width of screen buffer
   * \param scr_h     height of screen buffer
   * \param doc       initial content
   * \param w, h      initial size of the browser window
   */
  Browser_window(Factory *f,
      Scout_gfx::Document *content, Scout_gfx::View *pf,
      Area const &max_sz,
      int attr = ATTR_SIZER | ATTR_TITLEBAR);

  /**
   * Return visible document offset
   */
  inline int doc_offset()
  { return 10 + _panel->size().h() + (_attr & ATTR_TITLEBAR ? _TH : 0); }

  Widget  *curr_anchor();
  Browser *browser() { return this; }

  /**
   * Element interface
   */
  void draw(Mag_gfx::Canvas *c, Point const &p)
  {
    using Scout_gfx::Color;
    Basic_window::draw(c, p);

    if (_attr & ATTR_BORDER)
      {
	Color col(0, 0, 0);
	Rect g = Rect(p, geometry().area());
	c->draw_box(g.top(1), col);
	c->draw_box(g.bottom(1), col);
	c->draw_box(g.left(1), col);
	c->draw_box(g.right(1), col);
      }
  }
};

/*
 * \brief  Window interface
 * \author Norman Feske
 * \date   2006-08-30
 */

/*
 * Copyright (C) 2006-2009
 * Genode Labs, Feske & Helmuth Systementwicklung GbR
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _WINDOW_H_
#define _WINDOW_H_

#include <l4/mag-gfx/geometry>

#include <l4/scout-gfx/elements>
#include <l4/scout-gfx/redraw_manager>

#include "platform.h"

using namespace Scout_gfx;

/**********************
 ** Window interface **
 **********************/

class Scout_window : public Scout_gfx::Parent_element, public Window
{
private:

  Platform       *_pf;
  Area _max_sz;
  Redraw_manager *_redraw;   /* redraw manager       */

public:

  Scout_window(Platform *pf, Redraw_manager *redraw, Area const &max_sz)
  : _pf(pf), _max_sz(max_sz), _redraw(redraw)
  {
    /* init element attributes */
    _pos = Rect(Point(0,0), pf->geometry().area());
  }

  virtual ~Window() { }

  /**
   * Accessors
   */
  Platform *pf() const { return _pf; }
  Area max() const { return _max_sz; }
  Redraw_manager *redraw() const { return _redraw; }

  /**
   * Bring window to front
   */
  virtual void top() { _pf->top_view(); }

  /**
   * Element interface
   *
   * This function just collects the specified regions to be
   * redrawn but does not perform any immediate drawing
   * operation. The actual drawing must be initiated by
   * calling the process_redraw function.
   */
  void redraw_area(Rect const &r)
  { _redraw->request(r); }
};


/********************
 ** Event handlers **
 ********************/

class Drag_event_handler : public Scout_gfx::Event_handler
{
protected:

  int _key_cnt;     /* number of curr. pressed keys */
  Point _cm;  /* original mouse position      */
  Point _om;  /* current mouse positon        */

  virtual void start_drag() = 0;
  virtual void do_drag() = 0;

  Point diff() const { return _cm - _om; }

public:

  /**
   * Constructor
   */
  Drag_event_handler() { _key_cnt = 0; }

  /**
   * Event handler interface
   */
  void handle(Scout_gfx::Event &ev)
  {
    if (ev.type == Scout_gfx::Event::PRESS)   _key_cnt++;
    if (ev.type == Scout_gfx::Event::RELEASE) _key_cnt--;

    if (_key_cnt == 0) return;

    Point em(ev.mx, ev.my);
    /* first click starts dragging */
    if ((ev.type == Scout_gfx::Event::PRESS) && (_key_cnt == 1))
      {
	_cm = em;
	_om = em;
	start_drag();
      }

    /* check if mouse was moved */
    if (em == _cm) return;

    /* remember current mouse position */
    _cm = em;

    do_drag();
  }
};


class Sizer_event_handler : public Drag_event_handler
{
protected:

  Window *_window;
  Area _osz;   /* original window size */

  /**
   * Event handler interface
   */
  void start_drag()
  { _osz = _window->view_pos().area(); }

  void do_drag()
  {
    /* calculate new window size */
    Area nsz = _osz.grow(diff());

    _window->format(nsz);
  }

public:

  /**
   * Constructor
   */
  Sizer_event_handler(Window *window)
  {
    _window = window;
  }
};


class Mover_event_handler : public Drag_event_handler
{
protected:

  Window *_window;
  Point _ob;   /* original launchpad position */


  void start_drag()
  {
    _ob = _window->view_pos().p1();
    _window->top();
  }

  void do_drag()
  {
    Point nb = _ob + diff();

    _window->vpos(nb);
  }

public:

  /**
   * Constructor
   */
  Mover_event_handler(Window *window)
  {
    _window = window;
  }
};


#endif

/*
 * \brief   Scout tutorial browser main program
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

/**
 * Local includes
 */
#include "config.h"
#include "factory.h"
#include "platform.h"
#include <l4/cxx/exceptions>

#include <l4/scout-gfx/tick>
#include <l4/scout-gfx/redraw_manager>
#include <l4/scout-gfx/user_state>
#include <l4/scout-gfx/factory>
#include <l4/scout-gfx/redraw_manager>

#include "browser_window.h"
#include <iostream>
#include <l4/cxx/std_exc_io>

using Scout_gfx::Document;

extern Document *create_document(Scout_gfx::Factory *f);


/**
 * Runtime configuration
 */
namespace Config
{
	int iconbar_detail    = 1;
	int background_detail = 1;
	int mouse_cursor      = 0;
	int browser_attr      = 0;
}


#define POINTER_RGBA  _binary_pointer_rgba_start

extern unsigned char POINTER_RGBA[];

enum
{
  Browser_width  = 540,
  Browser_height = 660,
};

/**
 * Main program
 */
int main() //int argc, char **argv)
{
try {
  /* init platform */
  Platform *pf = Platform::get(Rect(Point(0,0), Area(Browser_width, Browser_height)));

  ::Factory *f = dynamic_cast< ::Factory* >(::Factory::set.find(*pf->pixel_info()));

  Document *doc = create_document(f);

  Scout_gfx::View *view = pf->create_view(Rect(Point(0,0), Area(Browser_width, Browser_height)));
  /* create instance of browser window */
  Scout_gfx::Basic_window *browser = 
    new Browser_window(f, 
	doc,    /* initial document       */
	view,   /* platform               */
	view->geometry().area(),  /* max size of window     */
	Config::browser_attr
	);

  /* initialize mouse cursor */
#if 0
  int mx = 0, my = 0;
  static Icon<Pixel_rgb565, 32, 32> mcursor;
  if (Config::mouse_cursor) {
      mcursor.geometry(mx, my, 32, 32);
      mcursor.rgba(POINTER_RGBA);
      mcursor.alpha(255);
      mcursor.findable(0);
      browser.append(&mcursor);
  }
#endif
  /* create user state manager */

  /* assign browser as root element to redraw manager */
  pf->root()->append(browser);
  browser->set_geometry(view->geometry());
  //browser->scroll_pos(Point(0, 0));
  browser->refresh();
  pf->process_redraws();

  using Scout_gfx::Event;

  browser->top();
  /* enter main loop */
  Event ev;
  unsigned long curr_time, old_time;
  curr_time = old_time = pf->timer_ticks();
  do {
      pf->get_event(&ev);

      if (ev.type != Event::WHEEL) {
#if 0
	  /* update mouse cursor */
	  if (Config::mouse_cursor && (ev.mx != mx || ev.my != my)) {
	      int x1 = min(ev.mx, mx);
	      int y1 = min(ev.my, my);
	      int x2 = max(ev.mx + mcursor.w() - 1, mx + mcursor.w() - 1);
	      int y2 = max(ev.my + mcursor.h() - 1, my + mcursor.h() - 1);

	      mcursor.geometry(ev.mx, ev.my, mcursor.w(), mcursor.h());
	      redraw.request(x1, y1, x2 - x1 + 1, y2 - y1 + 1);

	      mx = ev.mx; my = ev.my;
	  }
#endif
      }

      pf->root()->handle_event(ev);
#if 0
      if (ev.type == Event::REFRESH)
	pf->scr_update(Rect(Point(0, 0), pf->scr_size()));
#endif

      if (ev.type == Event::TIMER)
	Scout_gfx::Tick::handle(pf->timer_ticks());

      /* perform periodic redraw */
      curr_time = pf->timer_ticks();
      if (!pf->event_pending() && ((curr_time - old_time > 20) || (curr_time < old_time))) {
	  old_time = curr_time;
	  pf->process_redraws();
      }

  } while (ev.type != Event::QUIT);

  return 0;
} catch (L4::Runtime_error const &e) {
    std::cerr << "Fatal exception: " << e.str() << " '" << e.extra_str()
              << "'\n" << e;
}
return 1;
}

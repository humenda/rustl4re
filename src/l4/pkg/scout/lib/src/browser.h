/*
 * \brief   Browser interface
 * \date    2005-11-03
 * \author  Norman Feske <norman.feske@genode-labs.com>
 */

/*
 * Copyright (C) 2005-2009
 * Genode Labs, Feske & Helmuth Systementwicklung GbR
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _BROWSER_H_
#define _BROWSER_H_

#include <l4/scout-gfx/document>
#include <l4/scout-gfx/doc/link>

#include "browser_if.h"
#include "history.h"

using Mag_gfx::Area;
using Mag_gfx::Rect;
using Mag_gfx::Point;
using Scout_gfx::Widget;

extern Scout_gfx::Document *create_about();

class Scout_browser : public Scout_gfx::Browser, public Browser_if
{
protected:

  Scout_gfx::Document *_document;
  Scout_gfx::Document *_about;
  History   _history;
  int       _voffset;


  /**
   * Define content to present in browser window
   */
  virtual void _content(Scout_gfx::Parent_widget *content) = 0;

  /**
   * Request current content
   */
  virtual Scout_gfx::Parent_widget *_content() = 0;

public:

  /**
   * Constructor
   */
  explicit Scout_browser(int voffset = 0)
  : _document(0), _about(create_about()), _voffset(voffset)
  {}

  virtual ~Scout_browser() { }

  char const *title() const { return _document->title; }

  void voffset(int voffset)
  { _voffset = voffset; }

  /**
   * Travel backward in history
   *
   * \retval 1 success
   * \retval 0 end of history is reached
   */
  virtual bool go_backward()
  {
    _history.assign(curr_anchor());
    if (!_history.go(History::BACKWARD)) return 0;
    go_to(_history.curr(), 0);
    return 1;
  }

  /**
   * Follow history forward
   *
   * \retval 1 success,
   * \retval 0 end of history is reached
   */
  virtual bool go_forward()
  {
    _history.assign(curr_anchor());
    if (!_history.go(History::FORWARD)) return 0;
    go_to(_history.curr(), 0);
    return 1;
  }

  /**
   * Follow specified link location
   *
   * \param add_history  if set to 1, add new location to history
   */
  virtual void go_to(Widget *anchor, int add_history = 1)
  {
    if (!anchor) return;

    if (add_history)
      {
	_history.assign(curr_anchor());
	_history.add(anchor);
      }

    Scout_gfx::Parent_widget *new_content = Scout_gfx::Anchor::chapter(anchor);
    if (new_content)
      _content(new_content);

    //ypos(0);
    //ypos(_ypos - anchor->abs().y() + _voffset);

    if (new_content)
      {
	//new_content->curr_link_destination(0);
	new_content->refresh();
      }
  }

  /**
   * Get current anchor
   *
   * The current anchor is the element that is visible at the
   * top of the browser window. It depends on the scroll position.
   * We need to store these elements in the history to recover
   * the right viewport on the history entries even after
   * reformatting the document.
   */
  virtual Widget *curr_anchor() = 0;

  /**
   * Display table of contents
   */
  bool go_toc() { go_to(_document->toc, 1); return 1; }

  /**
   * Go to title page
   */
  bool go_home() { go_to(_document); return 1; }

  /**
   * Go to about page
   */
  bool go_about() { go_to(_about); return 1; }
};


#endif /* _BROWSER_H_ */
